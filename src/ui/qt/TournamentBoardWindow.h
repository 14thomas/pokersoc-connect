#pragma once

#include <QMainWindow>
#include <QString>

class QFrame;
class QGridLayout;
class QLabel;
class QScrollArea;
class QTimer;
class QVBoxLayout;
class QWidget;

namespace pokersoc::ui::qt {

class SessionContext;
class TournamentClock;

/// Full-screen TV / projector board. Host keeps the main window for buy-ins.
class TournamentBoardWindow : public QMainWindow {
    Q_OBJECT
public:
    TournamentBoardWindow(SessionContext& context, TournamentClock& clock,
                          QWidget* parent = nullptr);

    void showOnRoomDisplay(QWidget* host);
    void setBoardFullScreen(bool on);
    void refreshStats();

protected:
    void resizeEvent(QResizeEvent* event) override;

private:
    struct SideStat {
        QWidget* widget = nullptr;
        QLabel* caption = nullptr;
        QLabel* value = nullptr;
    };

    struct BoardRow {
        QWidget* widget = nullptr;
        QLabel* label = nullptr;
        QLabel* value = nullptr;
    };

    struct ChipCell {
        QWidget* widget = nullptr;
        QLabel* image = nullptr;
        QLabel* value = nullptr;
    };

    SideStat makeStat(const QString& caption, Qt::Alignment align = Qt::AlignLeft | Qt::AlignVCenter);
    BoardRow makeBoardRow(const QString& caption);
    QWidget* makeDivider();
    void applyFonts();
    void layoutLogo();
    void layoutColumns();
    int chipImageSize() const;
    void refreshWallClock();
    void refreshChips();
    void refreshPrizes(bool regoClosed);
    void addPrizeMarqueeRow(const QString& placeText, const QString& amountText);
    void measurePrizeLoop();
    void ensurePrizeScrollRunning();
    void tickPrizeScroll();
    void refreshAll();
    QFont boardFont(int pixelSize, bool bold = false) const;

    SessionContext& ctx_;
    TournamentClock& clock_;
    QTimer* wallTimer_ = nullptr;
    QTimer* prizeScrollTimer_ = nullptr;

    QWidget* logo_ = nullptr;
    QWidget* infoBlock_ = nullptr;
    QWidget* body_ = nullptr;
    QWidget* centreCol_ = nullptr;

    QLabel* name_ = nullptr;
    QLabel* wallClock_ = nullptr;
    QLabel* time_ = nullptr;
    QFrame* blindsBox_ = nullptr;
    BoardRow blindsRow_;
    BoardRow anteRow_;
    QFrame* nextBox_ = nullptr;
    BoardRow nextRow_;

    QWidget* leftCol_ = nullptr;
    QWidget* rightCol_ = nullptr;
    QWidget* leftDivider_ = nullptr;
    QWidget* rightDivider_ = nullptr;
    QWidget* leftTopStretch_ = nullptr;
    QWidget* leftBottomStretch_ = nullptr;

    QScrollArea* prizeScroll_ = nullptr;
    QWidget* prizeList_ = nullptr;
    QVBoxLayout* prizeLayout_ = nullptr;
    QLabel* prizeTitle_ = nullptr;
    bool prizesShowing_ = false;
    QString prizesFingerprint_;
    int prizeLoopHeight_ = 0;
    int prizeLoopCount_ = 0;

    QWidget* chipBlock_ = nullptr;
    QGridLayout* chipGrid_ = nullptr;
    ChipCell chipCells_[6];
    QString chipsFingerprint_;

    SideStat rego_;
    SideStat level_;
    SideStat timeToBreak_;
    SideStat breakLength_;
    SideStat avgStack_;
    SideStat players_;
    SideStat reentries_;
    SideStat total_;
    SideStat remaining_;
};

} // namespace pokersoc::ui::qt
