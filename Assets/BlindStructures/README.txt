BLIND STRUCTURE JSON
====================

Import / Save from Tournament Settings → Blind structure.

Format:
{
  "rounds": [
    { "minutes": 15, "sb": 100, "bb": 200, "bb_ante": 0 },
    { "minutes": 15, "break": true },
    ...
  ]
}

- sb / bb / bb_ante: chip amounts (numbers or strings like "1.5K")
- break: true for a break row (keeps prior blinds)
- PokerSoc_rounds_5h.json is the default structure for new tournaments
