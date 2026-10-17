#pragma once

#include <optional>

#include "domain/session/CashGameMode.h"    // CashGameConfig
#include "domain/session/SessionMode.h"      // SessionModeType
#include "domain/session/TournamentConfig.h" // TournamentConfig

namespace pokersoc::domain {
class DenominationCount;
class CashOut;
class BuyIn;
class DenominationTable;
class Cashbox;
class TransactionLog;
class Player;
class PlayerDirectory;
} // namespace pokersoc::domain

namespace pokersoc::persistence {

/// Abstract persistence boundary. The application layer records events here as
/// they happen, and asks it to rebuild the live objects on startup. Concrete
/// implementations (SQLite, in-memory, ...) are chosen in the composition root,
/// so the domain and services never depend on a specific storage engine.
class AppStore {
public:
    virtual ~AppStore() = default;

    /// Persist an opening / additional float (a tally of currency).
    virtual void recordFloat(const domain::DenominationCount& floatAmount) = 0;

    /// Persist a completed cash-out (chips in, cash out, tip).
    virtual void recordCashOut(const domain::CashOut& cashOut) = 0;

    /// Persist a completed buy-in / rebuy (cash in, change out).
    virtual void recordBuyIn(const domain::BuyIn& buyIn) = 0;

    /// Persist (insert or update) a player profile.
    virtual void recordPlayer(const domain::Player& player) = 0;

    // ---- session configuration (which mode this session runs, and its rules) ----
    virtual void saveSessionMode(domain::SessionModeType type) = 0;
    /// The stored mode, or nullopt if this is a brand-new session.
    virtual std::optional<domain::SessionModeType> loadSessionModeType() = 0;

    virtual void saveCashGameConfig(const domain::CashGameConfig& config) = 0;
    virtual domain::CashGameConfig loadCashGameConfig() = 0;

    virtual void saveTournamentConfig(const domain::TournamentConfig& config) = 0;
    virtual domain::TournamentConfig loadTournamentConfig() = 0;

    /// Replay everything stored, rebuilding the cashbox and transaction log.
    virtual void load(const domain::DenominationTable& table,
                      domain::Cashbox& cashbox,
                      domain::TransactionLog& log) = 0;

    /// Load every stored player into the directory.
    virtual void loadPlayers(domain::PlayerDirectory& directory) = 0;
};

} // namespace pokersoc::persistence
