# PokerSoc Connect — C++ Console Port

Object-oriented design plan for re-implementing the app in C++ as a
**console / terminal** program first. The GUI will be bolted on later, so the
whole design keeps *business logic* and *presentation* strictly separated.

---

## 1. Scope of this first pass

Translating everything at once is a trap. We start with the foundational
features that every other feature depends on:

| # | Feature | Why first |
|---|---------|-----------|
| 1 | Money & Denominations (notes, coins, chips, plaques) | Everything is counted in these |
| 2 | Players & Player directory | Needed by every transaction |
| 3 | Transactions: Buy-in, Rebuy, Cash-out | Core workflow |
| 4 | Cashbox (float + movements, totals, change-making) | Where the money lives |
| 5 | Session + modes (Cash game vs Tournament) + pricing | Drives buy-in rules |
| 6 | Console UI shell (menu + commands) | How we drive it for now |
| 7 | Repository abstractions (in-memory now, SQLite later) | Lets us swap storage |

**Deferred for later passes:** food catalog & sales, tips / lost chips,
activity log + undo, reporting / breakdowns, attendance export, admin password,
CSV / JSON import-export.

---

## 2. Architecture layers

The single most important rule (because the GUI comes later):

> The console is just **one** presentation of the application services.
> The domain and services know *nothing* about the console or any GUI.

```
+------------------------------------------------------------+
|  ui/console      ConsoleApp, Menu, Command, ConsoleIO      |  <- swappable
+------------------------------------------------------------+
|  application     PlayerService, BuyInService, CashOutSvc…   |  <- use cases
+------------------------------------------------------------+
|  domain          Money, Denomination, Player, Transaction, |  <- pure logic
|                  Cashbox, Session, PricingStrategy …        |     (no I/O)
+------------------------------------------------------------+
|  persistence     IPlayerRepository, ITransactionRepository |  <- interfaces
|                  InMemory*  (now)      Sqlite*  (later)     |     + impls
+------------------------------------------------------------+
         main.cpp = composition root (wires everything up)
```

When the GUI arrives, only the top layer is replaced; it calls the exact same
`application` services the console commands call today.

---

## 3. Class catalog

### 3.1 Value objects (`domain/money`)

- **`Money`** — immutable amount stored as `int64_t` cents.
  - `operator+ - * `, comparisons, `toString()` (e.g. `$12.50`), `fromCents`, `fromDollars`.
  - No floating point for money, ever.

- **`Denomination`** *(abstract base)* — one physical face value.
  - `cents()`, `label()`, `category()` (pure virtual), virtual `~Denomination()`.
  - **`CashDenomination`** *(abstract : Denomination)* — real currency.
    - **`Note`** — e.g. $5…$100.
    - **`Coin`** — e.g. 5c…$2.
  - **`ChipDenomination`** *(abstract : Denomination)* — gaming tokens.
    - `stackSize()` → 4 for 25c & $25, otherwise 5 (ports `ChipCounting`).
    - **`Chip`** — round chip.
    - **`Plaque`** — rectangular high-value token.

- **`DenominationTable`** — the ordered set of denominations in play
  (splits into currency vs chips), used to render count grids and make change.

### 3.2 People (`domain/player`)

- **`MembershipType`** `enum class { None, Bronze, Silver, Gold, Platinum, Diamond }`
- **`ArcStatus`** `enum class { Unknown, Arc, NonArc }`
- **`Player`** — entity keyed by `id` (scanned barcode).
  - name, email, studentNumber, degree, studyYear, `ArcStatus`, `MembershipType`,
    `isUnderage`, `createdAt`.
- **`PlayerDirectory`** — in-memory index of players (lookup / add / update),
  backed by an `IPlayerRepository`.

### 3.3 Transactions (`domain/transaction`) — the main class hierarchy

```
          Transaction  (abstract)
          /    |      \
      BuyIn  CashOut   (…future: FoodSale)
        |
      Rebuy            (a Rebuy *is a* BuyIn)
```

- **`Transaction`** *(abstract)* — `id`, `time`, `playerId`, `Money amount`,
  `staff`, `method`, `notes`.
  - `type()` → `TransactionType` (pure virtual).
  - `apply(Cashbox&)` / `reverse(Cashbox&)` (pure virtual) — how it moves money.
  - `describe()` for logs / receipts.
- **`BuyIn : Transaction`** — player hands in cash, receives chips.
- **`Rebuy : BuyIn`** — same mechanics, flagged as a rebuy (counts toward cap).
- **`CashOut : Transaction`** — player returns chips, receives cash (change-making).
- **`TransactionType`** `enum class { BuyIn, Rebuy, CashOut }`
- **`TransactionLog`** — ordered history; supports reversing the last entry (undo later).

### 3.4 Cashbox (`domain/cashbox`)

- **`MovementReason`** `enum class { BuyIn, CashOut, Adjust, FloatAdd, LostChip, Sale, Tip, Undo }`
- **`CashboxMovement`** — `denomination`, `deltaQty` (+in / −out), `reason`,
  optional `playerId` / `txId`, `batchId`, `time`.
- **`Cashbox`** — current quantity held per denomination (float + movements).
  - `add(denom, qty, reason…)`, `remove(...)`, `quantityOf(denom)`.
  - `total()` → `Money`, `breakdown()` → list of (denom, qty, subtotal).
  - `makeChange(Money)` → a set of denominations (greedy, respecting stock).

### 3.5 Session & pricing (`domain/session`) — Strategy pattern

```
     SessionMode (abstract)              PricingStrategy (abstract)
      /          \                         /          |            \
CashGameMode  TournamentMode   EqualValuePricing  ArcVsNonArcPricing  MembershipTierPricing
                   └── has-a ──────────────┘ (tournament only)
```

- **`SessionMode`** *(abstract)* — `name()`, `quoteBuyIn(player, isRebuy)` →
  `BuyInQuote` (either a fixed price or "amount is whatever cash is tendered").
  - **`CashGameMode`** — chips == cash 1:1; amount is free-form (tendered cash).
  - **`TournamentMode`** — holds a `PricingStrategy` + `RebuyPolicy`.
- **`PricingStrategy`** *(abstract)* — `priceFor(player, isRebuy)` → `Money`,
  `labelFor(player, isRebuy)`.
  - **`EqualValuePricing`** — placeholder for cash game (value == tendered).
  - **`ArcVsNonArcPricing`** — ports ARC vs non-ARC prices.
  - **`MembershipTierPricing`** — ports Bronze…Diamond prices.
- **`RebuyPolicy`** — `cap` (0 = unlimited), `canBuyIn(existingCount)`.
- **`Session`** — owns the active `SessionMode`, the `Cashbox`,
  the `TransactionLog`, and attendance; the hub the services operate on.

### 3.6 Application services (`application`) — GUI-agnostic use cases

These are what *both* the console and the future GUI call.

- **`PlayerService`** — find / create / update players.
- **`BuyInService`** — `buyIn(playerId, tenderedCash, isRebuy)`:
  validate (rebuy cap, underage), quote via `SessionMode`, update `Cashbox`,
  record a `BuyIn`/`Rebuy` in the log.
- **`CashOutService`** — `cashOut(playerId, returnedChips)`: value chips,
  make change from `Cashbox`, record `CashOut`.
- **`CashboxService`** — view totals / breakdown, add float.
- **`SessionService`** — start session, switch mode, configure tournament pricing.

### 3.7 Persistence (`persistence`) — Repository pattern

- **`IPlayerRepository`**, **`ITransactionRepository`**, **`ICashboxRepository`**
  *(abstract interfaces)*.
- **`InMemoryPlayerRepository`** … *(now — fast to build the console on)*.
- **`SqlitePlayerRepository`** … *(later — same interfaces, swapped in main.cpp)*.

Services depend on the **interfaces**, never the concrete storage → we can move
from in-memory to SQLite without touching domain or UI (Dependency Inversion).

### 3.8 Console UI (`ui/console`) — Command pattern

- **`IConsoleIO`** *(abstract)* → **`ConsoleIO`** (wraps `std::cin`/`std::cout`).
  Abstracting I/O means commands are testable and the GUI can reuse the logic.
- **`ConsoleApp`** — top-level loop; owns the services and the root `Menu`.
- **`Menu`** / **`MenuItem`** — render options, read a choice.
- **`Command`** *(abstract)* — `name()`, `execute(AppContext&)`.
  - **`BuyInCommand`**, **`CashOutCommand`**, **`ViewCashboxCommand`**,
    **`ManagePlayersCommand`**, **`SwitchModeCommand`**, **`QuitCommand`**.
  - Using Command objects now makes the future **undo** feature (and GUI buttons)
    a natural extension.

---

## 4. Design patterns in play

- **Strategy** — pricing (`PricingStrategy`) and session behaviour (`SessionMode`).
- **Command** — menu actions (and future undo / GUI buttons).
- **Repository** — storage behind interfaces (in-memory → SQLite).
- **Polymorphism / template method** — `Transaction::apply/reverse`.
- **Value objects** — `Money`, `Denomination` (immutable, comparable).
- **Dependency inversion** — services depend on abstractions, wired in `main.cpp`.

---

## 5. C++ conventions

- Namespace `pokersoc` (sub-namespaces per layer, e.g. `pokersoc::domain`).
- `enum class` for all enums; `int64_t` cents for money (no floats).
- Every abstract base gets a `virtual ~Base() = default`.
- Ownership via `std::unique_ptr`; pass dependencies by reference / `const&`.
- Prefer `std::optional`, `std::string_view`, `std::chrono` for time.
- Rule of zero where possible; mark overrides `override`.
- Build with CMake (added when we start writing code).

---

## 6. Suggested directory layout (filled in as we translate)

```
cpp/
  CMakeLists.txt
  src/
    main.cpp                      # composition root
    domain/
      money/        Money, Denomination, Note, Coin, Chip, Plaque, DenominationTable
      player/       Player, PlayerDirectory, enums
      transaction/  Transaction, BuyIn, Rebuy, CashOut, TransactionLog
      cashbox/      Cashbox, CashboxMovement
      session/      Session, SessionMode, PricingStrategy, RebuyPolicy
    application/     PlayerService, BuyInService, CashOutService, …
    persistence/     I*Repository + InMemory* (Sqlite* later)
    ui/console/      ConsoleApp, Menu, Command + subclasses, ConsoleIO
  include/           (public headers if we split interface/impl)
```

---

## 7. Suggested translation order

1. `Money` + `Denomination` hierarchy + `DenominationTable` (unit-test these).
2. `Player` + `PlayerDirectory` + `InMemoryPlayerRepository`.
3. `Cashbox` + movements + `makeChange`.
4. `Transaction` hierarchy + `TransactionLog`.
5. `SessionMode` / `PricingStrategy` + `Session`.
6. Services on top of the above.
7. Console shell (`ConsoleApp`, menu, commands) → first runnable terminal app.
8. Swap `InMemory*` for `Sqlite*` repositories.
9. Later passes: sales, tips, activity log + undo, reporting, GUI.
```
