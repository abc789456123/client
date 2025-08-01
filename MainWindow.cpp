#include "MainWindow.h"
#include "LineDrawingDialog.h"
#include "NetworkConfigDialog.h"
#include "EnvConfig.h"
#include "custommessagebox.h"
#include <QApplication>
#include <QStackedLayout>
#include <QMessageBox>
#include <QFileDialog>
#include <QStandardPaths>
#include <QDir>
#include <QDebug>
#include <QScreen>
#include <QGuiApplication>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QComboBox>
#include <QCalendarWidget>
#include <QDialog>

// ClickableImageLabel 구현
ClickableImageLabel::ClickableImageLabel(QWidget *parent)
    : QLabel(parent)
{
    setCursor(Qt::PointingHandCursor);
    setStyleSheet("border: 2px solid #ddd; border-radius: 8px; padding: 5px; background-color: white;");
}

void ClickableImageLabel::setImageData(const QString &imagePath, const QString &timestamp, const QString &logText)
{
    m_imagePath = imagePath;
    m_timestamp = timestamp;
    m_logText = logText;
}

void ClickableImageLabel::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        emit clicked(m_imagePath, m_timestamp, m_logText);
    }
    QLabel::mousePressEvent(event);
}

// MainWindow 구현
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_centralWidget(nullptr)
    , m_tabWidget(nullptr)
    , m_closeButton(nullptr)
    , m_liveVideoTab(nullptr)
    , m_videoStreamWidget(nullptr)
    , m_streamingButton(nullptr)
    , m_capturedImageTab(nullptr)
    , m_imageScrollArea(nullptr)
    , m_imageGridWidget(nullptr)
    , m_imageGridLayout(nullptr)
    , m_dateButton(nullptr)
    , m_calendarWidget(nullptr)
    , m_calendarDialog(nullptr)
    , m_hourComboBox(nullptr)
    , m_dateEdit(nullptr)
    , m_hourSpinBox(nullptr)
    , m_requestButton(nullptr)
    //, m_statusLabel(nullptr)
    , m_networkButton(nullptr)
    , m_rtspUrl("")  // 빈 문자열로 초기화
    , m_tcpHost("")  // 빈 문자열로 초기화
    , m_tcpPort(0)   // 0으로 초기화
    , m_isConnected(false)
    , m_tcpCommunicator(nullptr)
    , m_networkManager(nullptr)
    , m_updateTimer(nullptr)
    , m_requestTimeoutTimer(nullptr)
    , m_imageViewerDialog(nullptr)
    , m_networkDialog(nullptr)
    , m_lineDrawingDialog(nullptr)
{
    // .env 파일 로드
    EnvConfig::loadFromFile(".env");

    // .env에서 네트워크 설정 로드
    m_rtspUrl = EnvConfig::getValue("RTSP_URL", "rtsp://192.168.0.81:8554/original");
    m_tcpHost = EnvConfig::getValue("TCP_HOST", "192.168.0.81");
    m_tcpPort = EnvConfig::getValue("TCP_PORT", "8080").toInt();

    qDebug() << "[MainWindow] .env 설정 로드됨 - RTSP:" << m_rtspUrl << "TCP:" << m_tcpHost << ":" << m_tcpPort;

    // 선택된 날짜 초기화
    m_selectedDate = QDate::currentDate();

    // UI 설정
    setupUI();

    // 네트워크 연결 설정
    setupNetworkConnection();

    // 스타일 적용
    applyStyles();

    // 화면 크기 가져오기
    QScreen *screen = QGuiApplication::primaryScreen();
    QRect screenGeometry = screen->availableGeometry();


    int fixedWidth = 1000;
    int fixedHeight = 670;

    setFixedSize(fixedWidth, fixedHeight);

    // 화면 중앙에 배치
    move((screenGeometry.width() - fixedWidth) / 2,
         (screenGeometry.height() - fixedHeight) / 2);
}

MainWindow::~MainWindow()
{
    if (m_updateTimer) {
        m_updateTimer->stop();
    }
    if (m_requestTimeoutTimer) {
        m_requestTimeoutTimer->stop();
    }
    if (m_calendarDialog) {
        delete m_calendarDialog;
    }
}

// TCP 통신기 설정 메서드
void MainWindow::setTcpCommunicator(TcpCommunicator* communicator)
{
    // 기존 연결 해제
    if (m_tcpCommunicator && m_tcpCommunicator != communicator) {
        disconnect(m_tcpCommunicator, &TcpCommunicator::connected,
                   this, &MainWindow::onTcpConnected);
        disconnect(m_tcpCommunicator, &TcpCommunicator::disconnected,
                   this, &MainWindow::onTcpDisconnected);
        disconnect(m_tcpCommunicator, &TcpCommunicator::errorOccurred,
                   this, &MainWindow::onTcpError);
        disconnect(m_tcpCommunicator, &TcpCommunicator::messageReceived,
                   this, &MainWindow::onTcpDataReceived);
        disconnect(m_tcpCommunicator, &TcpCommunicator::imagesReceived,
                   this, &MainWindow::onImagesReceived);
        disconnect(m_tcpCommunicator, &TcpCommunicator::coordinatesConfirmed,
                   this, &MainWindow::onCoordinatesConfirmed);
        disconnect(m_tcpCommunicator, &TcpCommunicator::statusUpdated,
                   this, &MainWindow::onStatusUpdated);
        disconnect(m_tcpCommunicator, &TcpCommunicator::perpendicularLineConfirmed,
                   this, nullptr);
    }

    m_tcpCommunicator = communicator;

    // 새로운 통신기에 시그널 연결
    if (m_tcpCommunicator) {
        connect(m_tcpCommunicator, &TcpCommunicator::connected,
                this, &MainWindow::onTcpConnected);
        connect(m_tcpCommunicator, &TcpCommunicator::disconnected,
                this, &MainWindow::onTcpDisconnected);
        connect(m_tcpCommunicator, &TcpCommunicator::errorOccurred,
                this, &MainWindow::onTcpError);
        connect(m_tcpCommunicator, &TcpCommunicator::messageReceived,
                this, &MainWindow::onTcpDataReceived);
        connect(m_tcpCommunicator, &TcpCommunicator::imagesReceived,
                this, &MainWindow::onImagesReceived);
        connect(m_tcpCommunicator, &TcpCommunicator::coordinatesConfirmed,
                this, &MainWindow::onCoordinatesConfirmed);
        connect(m_tcpCommunicator, &TcpCommunicator::statusUpdated,
                this, &MainWindow::onStatusUpdated);
        connect(m_tcpCommunicator, &TcpCommunicator::perpendicularLineConfirmed,
                this, [this](bool success, const QString &message) {
                    qDebug() << "수직선 서버 응답 - 성공:" << success << "메시지:" << message;
                    if (success) {
                        CustomMessageBox msgBox(nullptr, "수직선 전송 완료", "수직선이 성공적으로 서버에 전송되었습니다.");
                        msgBox.setFixedSize(300,150);
                        msgBox.exec();
                    } else {
                        CustomMessageBox msgBox(nullptr, "수직선 전송 실패", "수직선 전송에 실패했습니다: " + message);
                        msgBox.setFixedSize(300,150);
                        msgBox.exec();
                    }
                });
    }
}

void MainWindow::setupUI()
{
    m_centralWidget = new QWidget(this);
    setCentralWidget(m_centralWidget);


    QVBoxLayout *mainLayout = new QVBoxLayout(m_centralWidget);
    // 헤더 영역
    QWidget *headerBar = new QWidget();
    headerBar->setFixedHeight(50);
    headerBar->setStyleSheet("background-color: #292d41;");


    QGridLayout *headerLayout = new QGridLayout(headerBar);
    headerLayout->setContentsMargins(5, 0, 5, 0);
    headerLayout->setHorizontalSpacing(5);


    QLabel* titleLabel = new QLabel("CCTV Monitoring System");
    titleLabel->setAlignment(Qt::AlignCenter);
    titleLabel->setStyleSheet("color: white; font-size: 24px; font-weight: bold;");
    // headerLayout->addWidget(titleLabel);

    headerLayout->addWidget(titleLabel, 0, 0, 1, 3, Qt::AlignHCenter);


    // 네트워크 버튼
    m_networkButton = new QPushButton();
    m_networkButton->setIcon(QIcon(":/icons/NetworkConnect.png")); // 아이콘 리소스 확인
    m_networkButton->setIconSize(QSize(25, 25));
    m_networkButton->setFixedSize(40, 40);
    m_networkButton->setStyleSheet(
        "QPushButton { background-color: transparent; border: none; } "
        "QPushButton:hover { background-color: rgba(255,255,255,0.1); border-radius: 20px; }");

    //닫기 버튼
    m_closeButton = new QPushButton();
    m_closeButton->setIcon(QIcon(":/icons/close.png"));
    m_closeButton->setIconSize(QSize(20,20));
    m_closeButton->setFixedSize(40, 40);
    m_closeButton->setStyleSheet("QPushButton { background-color: transparent; border: none; } "
                                 "QPushButton:hover { background-color: rgba(255,255,255,0.1); border-radius: 20px; }");


    // 버튼 컨테이너 (수평 배치)
    QHBoxLayout *rightButtonsLayout = new QHBoxLayout();
    rightButtonsLayout->setSpacing(5);
    rightButtonsLayout->addWidget(m_networkButton);
    rightButtonsLayout->addWidget(m_closeButton);

    // 오른쪽 영역 위젯으로 감싸기
    QWidget *rightButtonsWidget = new QWidget();
    rightButtonsWidget->setLayout(rightButtonsLayout);
    headerLayout->addWidget(rightButtonsWidget, 0, 2, Qt::AlignRight | Qt::AlignVCenter);

    // 열 너비 균형
    headerLayout->setColumnStretch(0, 1);
    headerLayout->setColumnStretch(1, 2); // 가운데 강조
    headerLayout->setColumnStretch(2, 1);

    mainLayout->addWidget(headerBar);

    // 콘텐츠 영역
    QHBoxLayout *contentLayout = new QHBoxLayout();

    m_tabWidget = new QTabWidget();

    m_tabWidget->setStyleSheet("QTabWidget::pane {background-color: #474B5C; } "
                               "QTabBar::tab { background-color: #666977; color : white; padding: 10px 20px; border-top-left-radius: 15px; border-top-right-radius: 15px;} "
                               "QTabBar::tab:selected { background-color: #474B5C; color : #F37321; border-bottom: 2px solid #474B5C; }");

    setupLiveVideoTab();
    setupCapturedImageTab();
    contentLayout->addWidget(m_tabWidget, 3);

    // 사이드바
    QWidget *sidebarWidget = new QWidget();
    sidebarWidget->setMinimumWidth(250);
    sidebarWidget->setMaximumWidth(350);
    sidebarWidget->setStyleSheet("background-color: #292d41;");

    QVBoxLayout *sidebarLayout = new QVBoxLayout(sidebarWidget);

    m_modeComboBox = new QComboBox();
    sidebarLayout->addWidget(m_modeComboBox);

    mainLayout->addLayout(contentLayout);

    connect(m_networkButton,&QPushButton::clicked,this,&MainWindow::onNetworkConfigClicked);
    connect(m_closeButton, &QPushButton::clicked, this, &MainWindow::close);
}

void MainWindow::setupLiveVideoTab()
{

    m_liveVideoTab = new QWidget();
    m_liveVideoTab->setStyleSheet("background-color: #474B5C;");

    QVBoxLayout *layout = new QVBoxLayout(m_liveVideoTab);
    layout->setSpacing(0);
    layout->setContentsMargins(5, 5, 5, 5);

    // 컨테이너 + 오버레이 구조
    QWidget *videoContainer = new QWidget();
    QStackedLayout *stackedLayout = new QStackedLayout(videoContainer);

    m_videoStreamWidget = new VideoStreamWidget();
    m_videoStreamWidget->setMinimumHeight(400);

    // 재생 버튼 (아이콘 이미지 사용)
    QPushButton *playOverlayButton = new QPushButton();
    playOverlayButton->setIcon(QIcon(":/icons/play1.png"));  //재생 이미지
    playOverlayButton->setIconSize(QSize(48, 48));
    playOverlayButton->setFixedSize(64, 48);
    playOverlayButton->setCursor(Qt::PointingHandCursor);
    playOverlayButton->setStyleSheet(
        "QPushButton { background-color: #f37321; border: none; border-radius: 12px; } "
        "QPushButton:hover { background-color: #fa8a3f; }"
        );

    // 버튼을 가운데 정렬할 수 있도록 wrap 위젯
    QWidget *overlayWidget = new QWidget();
    QVBoxLayout *overlayLayout = new QVBoxLayout(overlayWidget);
    overlayLayout->addStretch();
    overlayLayout->addWidget(playOverlayButton, 0, Qt::AlignHCenter);
    overlayLayout->addStretch();
    overlayLayout->setContentsMargins(0, 0, 0, 0);

    // ▶ 버튼 클릭 시: 영상 시작 + 영상 위젯을 전면에
    connect(playOverlayButton, &QPushButton::clicked, this, [=]() {
        if (!m_rtspUrl.isEmpty()) {
            m_videoStreamWidget->startStream(m_rtspUrl);
            stackedLayout->setCurrentWidget(m_videoStreamWidget);
        } else {
            CustomMessageBox msgBox(nullptr, "RTSP URL 누락", "먼저 네트워크 설정에서 RTSP URL을 입력하세요.");
            msgBox.setFixedSize(300,150);
            msgBox.exec();
        }
    });

    // 영상 클릭 시: 정지 + ▶ 버튼 레이어 다시 앞으로
    connect(m_videoStreamWidget, &VideoStreamWidget::clicked, this, [=]() {
        if (m_videoStreamWidget->isStreaming()) {
            m_videoStreamWidget->stopStream();
            stackedLayout->setCurrentWidget(overlayWidget);  // 오버레이를 다시 위로
        }
    });

    stackedLayout->addWidget(overlayWidget);
    stackedLayout->addWidget(m_videoStreamWidget);

    // 클릭하면 재생 + 버튼 숨김
    connect(playOverlayButton, &QPushButton::clicked, this, [=]() {
        if (!m_rtspUrl.isEmpty()) {
            m_videoStreamWidget->startStream(m_rtspUrl);
            stackedLayout->setCurrentWidget(m_videoStreamWidget);  // 영상 보여주기
        } else {
            CustomMessageBox msgBox(nullptr, "RTSP URL 누락", "먼저 네트워크 설정에서 RTSP URL을 입력하세요.");
            msgBox.setFixedSize(300,150);
            msgBox.exec();
        }
    });
    // event 연결
    connect(m_videoStreamWidget, &VideoStreamWidget::clicked, this, &MainWindow::onVideoStreamClicked);
    connect(m_videoStreamWidget, &VideoStreamWidget::streamError, this, &MainWindow::onStreamError);
    //drawButton
    connect(m_videoStreamWidget, &VideoStreamWidget::drawButtonClicked,
            this, &MainWindow::onDrawButtonClicked);

    // 레이아웃 적용
    layout->addWidget(videoContainer);

    m_tabWidget->addTab(m_liveVideoTab, "Live Video Stream");
}

void MainWindow::onDrawButtonClicked()
{
    if (!m_videoStreamWidget->isStreaming()) {
        CustomMessageBox msgBox(nullptr, "안내", "먼저 스트리밍을 시작해주세요.");
        msgBox.setFixedSize(300,150);
        msgBox.exec();
        return;
    }

    if (!m_lineDrawingDialog) {
        m_lineDrawingDialog = new LineDrawingDialog(m_rtspUrl, m_tcpCommunicator, this);

        connect(m_lineDrawingDialog, &LineDrawingDialog::lineCoordinatesReady,
                this, [this](int x1, int y1, int x2, int y2) {
                    this->sendSingleLineCoordinates(x1, y1, x2, y2);
                });

        connect(m_lineDrawingDialog, &LineDrawingDialog::categorizedLinesReady,
                this, [this](const QList<RoadLineData> &roadLines, const QList<DetectionLineData> &detectionLines) {
                    this->sendCategorizedCoordinates(roadLines, detectionLines);
                });

        connect(m_lineDrawingDialog, &LineDrawingDialog::perpendicularLineGenerated,
                this, [this](int detectionLineIndex, double a, double b) {
                    if (m_tcpCommunicator && m_tcpCommunicator->isConnectedToServer()) {
                        PerpendicularLineData perpData{detectionLineIndex, a, b};
                        if (m_tcpCommunicator->sendPerpendicularLine(perpData)) {
                            qDebug() << "수직선 전송 성공";
                        } else {
                            CustomMessageBox msgBox(nullptr, "전송 실패", "수직선 전송에 실패했습니다.");
                            msgBox.setFixedSize(300,150);
                            msgBox.exec();
                        }
                    }
                });
    }

    m_lineDrawingDialog->exec();
}

void MainWindow::setupCapturedImageTab()
{
    m_capturedImageTab = new QWidget();
    m_capturedImageTab->setStyleSheet("background-color: #474B5C;");

    QVBoxLayout *mainLayout = new QVBoxLayout(m_capturedImageTab);
    mainLayout->setContentsMargins(15, 15, 15, 15);

    // 상단 컨트롤 바
    QWidget *topBar = new QWidget();
    topBar->setStyleSheet("background-color: #474B5C;");
    QHBoxLayout *topLayout = new QHBoxLayout(topBar);
    topLayout->setContentsMargins(0, 0, 0, 0);
    topLayout->setSpacing(10);

    // 날짜 라벨
    QLabel *dateLabel = new QLabel("날짜:");
    dateLabel->setStyleSheet("color: white; font-weight: bold;");
    topLayout->addWidget(dateLabel);


    // 날짜 선택
    m_dateEdit = new QDateEdit(QDate::currentDate());
    m_dateEdit->setDisplayFormat("yyyy-MM-dd");
    m_dateEdit->setCalendarPopup(true);
    m_dateEdit->setStyleSheet(
        "QDateEdit {"
        " background-color: #383A41;"
        " color: white;"
        " padding: 6px 12px;"
        " border: none;"
        " border-radius: 15px;"
        "}"
        "QDateEdit::drop-down {"
        " background-color: #383A41;"
        " width: 24px;"
        " border: none;"
        " border-top-right-radius: 15px;"
        " border-bottom-right-radius: 15px;"
        "}"
        "QDateEdit::down-arrow {"
        " image: url(:/icons/up_down.png);"  // 너가 넣은 화살표 아이콘 경로
        " width: 20px;"
        " height: 20px;"
        "}"
        "QDateEdit QAbstractItemView {"
        " background-color: #2c2c2c;"
        " color: white;"
        " selection-background-color: #505050;"
        " border: none;"
        "}"
        );

    topLayout->addWidget(m_dateEdit);
    connect(m_dateEdit, &QDateEdit::dateChanged, this, [this](const QDate &newDate) {
        // 새로운 날짜(newDate)를 사용해 원하는 작업을 수행
        qDebug() << "날짜가 변경되었습니다: " << newDate.toString("yyyy-MM-dd");
        m_selectedDate = newDate;
        // 다른 UI 업데이트나 로직을 여기에 추가할 수 있습니다.
    });

    // 달력 다이얼로그 설정
    m_calendarDialog = new QDialog(this);
    m_calendarDialog->setWindowTitle("날짜 선택");
    m_calendarDialog->setModal(true);
    m_calendarDialog->setFixedSize(380, 350);
    m_calendarDialog->setStyleSheet(R"(
    QCalendarWidget QToolButton {
        background-color: #444857;
        color: white;
        font-weight: bold;
        border: none;
        margin: 5px;
        height: 30px;
    }

    QCalendarWidget QToolButton::left-arrow {
        image: url(:/icons/left.png);  /* 왼쪽 화살표 이미지 */
        width: 50px;
        height: 50px;
    }

    QCalendarWidget QToolButton::right-arrow {
        image: QIcon(":/icons/right.png");  /* 오른쪽 화살표 이미지 */
        width: 24px;
        height: 24px;
    }


    QCalendarWidget QToolButton:hover {
        background-color: #5a5e6e;
    }

    QCalendarWidget QHeaderView::section {
        background-color: #3d4251;
        color: #cfcfcf;
        font-weight: bold;
        border: none;
        padding: 5px;
    }

    QCalendarWidget QTableView {
        background-color: #2c2f38;
        color: white;
        selection-background-color: #F37321;
        selection-color: black;
        gridline-color: #888;
    }
)");

    QVBoxLayout *calendarLayout = new QVBoxLayout(m_calendarDialog);

    m_calendarWidget = new QCalendarWidget();
    m_calendarWidget->setSelectedDate(m_selectedDate);
    m_calendarWidget->setStyleSheet("background-color:#292D41;");
    connect(m_calendarWidget, &QCalendarWidget::clicked, this, &MainWindow::onCalendarDateSelected);
    calendarLayout->addWidget(m_calendarWidget);

    // 시간 라벨
    QLabel *timeLabel = new QLabel("시간:");
    timeLabel->setStyleSheet("color: white; font-weight: bold;");
    topLayout->addWidget(timeLabel);

    // 시간 선택
    m_hourComboBox = new QComboBox();
    for (int h = 0; h < 24; ++h)
        m_hourComboBox->addItem(QString("%1시 ~ %2시").arg(h, 2, 10, QChar('0')).arg(h + 1, 2, 10, QChar('0')), h);
    m_hourComboBox->setCurrentIndex(QTime::currentTime().hour());
    m_hourComboBox->setStyleSheet(
        "QComboBox {"
        " background-color: #383A41;"
        " color: white;"
        " padding: 6px 12px;"
        " border: none;"
        " border-radius: 15px;"
        "}"
        "QComboBox::drop-down {"
        " background-color: #383A41;"
        " width: 24px;"
        " border: none;"
        " border-top-right-radius: 15px;"
        " border-bottom-right-radius: 15px;"
        "}"
        "QComboBox::down-arrow {"
        " image: url(:/icons/up_down.png);"  // 화살표 아이콘
        " width: 20px;"
        " height: 20px;"
        "}"
        "QComboBox QAbstractItemView {"
        " background-color: #2c2c2c;"
        " color: white;"
        " selection-background-color: #505050;"
        " border: none;"
        "}"
        );
    m_hourComboBox->setCurrentIndex(QTime::currentTime().hour());
    connect(m_hourComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onHourComboChanged);
    topLayout->addWidget(m_hourComboBox);

    // load 버튼
    m_requestButton = new QPushButton("load");
    m_requestButton->setStyleSheet(
        "QPushButton { background-color: #f37321; color: white; padding: 6px 16px; border-radius: 4px; font-weight: bold; }"
        "QPushButton:hover { background-color: #f89b6c; }"
        "QPushButton:disabled { background-color: #aaa; }"
        );
    //버튼 클릭 신호 연결
    connect(m_requestButton, &QPushButton::clicked, this, &MainWindow::onRequestImagesClicked);
    topLayout->addWidget(m_requestButton);
    topLayout->addStretch(); // 오른쪽 여백 확보

    mainLayout->addWidget(topBar);
    // 이미지 영역
    m_imageScrollArea = new QScrollArea();
    m_imageScrollArea->setWidgetResizable(true);
    m_imageScrollArea->setStyleSheet("QScrollArea { background-color: #474B5C; border: none;}");

    m_imageGridWidget = new QWidget();
    m_imageGridWidget->setStyleSheet("background-color: #474B5C; border: none;");
    m_imageGridLayout = new QGridLayout(m_imageGridWidget);
    m_imageGridLayout->setSpacing(15);
    m_imageGridLayout->setContentsMargins(15, 15, 15, 15);

    QLabel *emptyLabel = new QLabel("이미지 요청 버튼을 눌러 해당 시간대의 이미지를 불러오세요.");
    emptyLabel->setAlignment(Qt::AlignCenter);
    emptyLabel->setStyleSheet("color: #999; font-size: 16px; padding: 50px;");
    m_imageGridLayout->addWidget(emptyLabel, 0, 0, 1, 2);

    m_imageScrollArea->setWidget(m_imageGridWidget);
    mainLayout->addWidget(m_imageScrollArea);

    m_tabWidget->addTab(m_capturedImageTab, "Captured Images");
}



void MainWindow::setupNetworkConnection()
{
    m_networkManager = new QNetworkAccessManager(this);

    m_updateTimer = new QTimer(this);
    connect(m_updateTimer, &QTimer::timeout, this, &MainWindow::updateLogDisplay);
    m_updateTimer->start(5000);

    // TcpCommunicator는 외부에서 설정되므로 여기서 생성하지 않음
    // m_tcpCommunicator = new TcpCommunicator(this);

    // 이미 설정된 TcpCommunicator가 있는 경우에만 시그널 연결
    if (m_tcpCommunicator) {
        // 수정된 시그널 연결 - 개별 시그널로 분리
        connect(m_tcpCommunicator, &TcpCommunicator::connected, this, &MainWindow::onTcpConnected);
        connect(m_tcpCommunicator, &TcpCommunicator::disconnected, this, &MainWindow::onTcpDisconnected);
        connect(m_tcpCommunicator, &TcpCommunicator::errorOccurred, this, &MainWindow::onTcpError);
        connect(m_tcpCommunicator, &TcpCommunicator::messageReceived, this, &MainWindow::onTcpDataReceived);
        connect(m_tcpCommunicator, &TcpCommunicator::imagesReceived, this, &MainWindow::onImagesReceived);

        // 새로운 JSON 기반 시그널 연결
        connect(m_tcpCommunicator, &TcpCommunicator::coordinatesConfirmed, this, &MainWindow::onCoordinatesConfirmed);
        connect(m_tcpCommunicator, &TcpCommunicator::statusUpdated, this, &MainWindow::onStatusUpdated);

        // 수직선 확인 시그널 연결 추가
        connect(m_tcpCommunicator, &TcpCommunicator::perpendicularLineConfirmed,
                this, [this](bool success, const QString &message) {
                    qDebug() << "수직선 서버 응답 - 성공:" << success << "메시지:" << message;

                    if (success) {
                        CustomMessageBox msgBox(nullptr, "수직선 전송 완료", "수직선이 성공적으로 서버에 전송되었습니다.");
                        msgBox.setFixedSize(300,150);
                        msgBox.exec();
                    } else {
                        CustomMessageBox msgBox(nullptr, "수직선 전송 실패", "수직선 전송에 실패했습니다: " + message);
                        msgBox.setFixedSize(300,150);
                        msgBox.exec();
                    }
                });
    }

    // 이미지 요청 타임아웃 타이머 설정
    m_requestTimeoutTimer = new QTimer(this);
    m_requestTimeoutTimer->setSingleShot(true);
    m_requestTimeoutTimer->setInterval(30000);
    connect(m_requestTimeoutTimer, &QTimer::timeout, this, &MainWindow::onRequestTimeout);

    m_imageViewerDialog = new ImageViewerDialog(this);
}

void MainWindow::applyStyles()
{
    setStyleSheet("QMainWindow { background-color: #292d41; }");
}


void MainWindow::clearImageGrid()
{
    QLayoutItem *item;
    while ((item = m_imageGridLayout->takeAt(0)) != nullptr) {
        delete item->widget();
        delete item;
    }
}

void MainWindow::displayImages(const QList<ImageData> &images)
{
    clearImageGrid();

    if (images.isEmpty()) {
        QLabel *emptyLabel = new QLabel("해당 시간대에 캡처된 이미지가 없습니다.");
        emptyLabel->setAlignment(Qt::AlignCenter);
        emptyLabel->setStyleSheet("color: #999; font-size: 16px; padding: 50px;");
        m_imageGridLayout->addWidget(emptyLabel, 0, 0, 1, 2);
        return;
    }

    int row = 0;
    int col = 0;

    for (const ImageData &imageData : images) {
        ClickableImageLabel *imageLabel = new ClickableImageLabel();
        imageLabel->setFixedSize(300, 200);
        imageLabel->setScaledContents(true);
        imageLabel->setImageData(imageData.imagePath, imageData.timestamp, imageData.logText);

        QPixmap pixmap;
        if (pixmap.load(imageData.imagePath)) {
            imageLabel->setPixmap(pixmap);
        } else {
            imageLabel->setText("이미지 로드 실패");
            imageLabel->setStyleSheet(imageLabel->styleSheet() + " color: #999;");
        }

        QLabel *timeLabel = new QLabel(imageData.timestamp);
        timeLabel->setAlignment(Qt::AlignCenter);
        timeLabel->setStyleSheet("background-color: #383A41; color: white; padding: 5px; font-size: 12px;");

        QWidget *container = new QWidget();
        container->setFixedSize(320, 240);

        // 이미지 스타일 적용
        container->setStyleSheet(
            "background-color: #383A41;"
            "border-radius: 10px;"
            "padding: 5px;"
            );
        QVBoxLayout *containerLayout = new QVBoxLayout(container);
        containerLayout->setContentsMargins(5, 5, 5, 5);
        containerLayout->setSpacing(8);
        containerLayout->addWidget(imageLabel);
        containerLayout->addWidget(timeLabel);

        connect(imageLabel, &ClickableImageLabel::clicked, this, &MainWindow::onImageClicked);

        m_imageGridLayout->addWidget(container, row, col);

        col++;
        if (col >= 2) {
            col = 0;
            row++;
        }
    }

    m_imageGridWidget->adjustSize();
}


void MainWindow::onNetworkConfigClicked()
{
    if (!m_networkDialog) {
        m_networkDialog = new NetworkConfigDialog(this);
        m_networkDialog->setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
        m_networkDialog->setRtspUrl(m_rtspUrl);
        m_networkDialog->setTcpHost(m_tcpHost);
        m_networkDialog->setTcpPort(m_tcpPort);
    }

    if (m_networkDialog->exec() == QDialog::Accepted) {
        m_rtspUrl = m_networkDialog->getRtspUrl();
        m_tcpHost = m_networkDialog->getTcpHost();
        m_tcpPort = m_networkDialog->getTcpPort();

        if (m_videoStreamWidget) {
            m_videoStreamWidget->setStreamUrl(m_rtspUrl);
        }

        if (m_tcpCommunicator) {
            m_tcpCommunicator->connectToServer(m_tcpHost, m_tcpPort);
        }

        qDebug() << "네트워크 설정 업데이트:" << m_rtspUrl << m_tcpHost << m_tcpPort;
    }
}

void MainWindow::onVideoStreamClicked()
{
    if (!m_videoStreamWidget->isStreaming()) {
        CustomMessageBox msgBox(nullptr, "안내", "먼저 스트리밍을 시작해주세요.");
        msgBox.setFixedSize(300,150);
        msgBox.exec();
        return;
    }

    if (!m_lineDrawingDialog) {
        // TcpCommunicator를 직접 전달
        m_lineDrawingDialog = new LineDrawingDialog(m_rtspUrl, m_tcpCommunicator, this);
        m_lineDrawingDialog->setWindowFlags(Qt::Window | Qt::FramelessWindowHint);

        // 기존 시그널 연결
        connect(m_lineDrawingDialog, &LineDrawingDialog::lineCoordinatesReady,
                this, [this](int x1, int y1, int x2, int y2) {
                    this->sendSingleLineCoordinates(x1, y1, x2, y2);
                });

        // 새로운 카테고리별 좌표 시그널 연결 추가
        connect(m_lineDrawingDialog, &LineDrawingDialog::categorizedLinesReady,
                this, [this](const QList<RoadLineData> &roadLines, const QList<DetectionLineData> &detectionLines) {
                    this->sendCategorizedCoordinates(roadLines, detectionLines);
                });

        // 수직선 생성 시그널 연결 추가
        connect(m_lineDrawingDialog, &LineDrawingDialog::perpendicularLineGenerated,
                this, [this](int detectionLineIndex, double a, double b) {
                    if (m_tcpCommunicator && m_tcpCommunicator->isConnectedToServer()) {
                        PerpendicularLineData perpData;
                        perpData.index = detectionLineIndex;
                        perpData.a = a;
                        perpData.b = b;

                        bool success = m_tcpCommunicator->sendPerpendicularLine(perpData);
                        if (success) {
                            qDebug() << "수직선 전송 성공 - index:" << detectionLineIndex
                                     << "y = " << a << "x + " << b;
                        } else {
                            qDebug() << "수직선 전송 실패";
                            CustomMessageBox msgBox(nullptr, "전송 실패", "수직선 데이터 전송에 실패했습니다.");
                            msgBox.setFixedSize(300,150);
                            msgBox.exec();
                        }
                    } else {
                        CustomMessageBox msgBox(nullptr, "연결 오류", "서버에 연결되어 있지 않습니다.");
                        msgBox.setFixedSize(300,150);
                        msgBox.exec();
                    }
                });
    }

    m_lineDrawingDialog->exec();
}

void MainWindow::sendMultipleLineCoordinates(const QList<QPair<QPoint, QPoint>> &lines)
{
    if (m_tcpCommunicator && m_tcpCommunicator->isConnectedToServer()) {
        for (int i = 0; i < lines.size(); ++i) {
            const auto &line = lines[i];
            m_tcpCommunicator->sendLineCoordinates(line.first.x(), line.first.y(), line.second.x(), line.second.y());
            qDebug() << QString("기준선 %1 좌표 전송 성공:").arg(i + 1) << line.first << "to" << line.second;
        }

        CustomMessageBox msgBox(nullptr, "전송 완료",
                                QString("%1개의 기준선 좌표가 서버로 전송되었습니다.").arg(lines.size()));
        msgBox.setFixedSize(300,150);
        msgBox.exec();
    } else {
        qDebug() << "TCP 연결이 없어 좌표 전송 실패";
        CustomMessageBox msgBox(nullptr, "전송 실패", "서버에 연결되어 있지 않습니다.");
        msgBox.setFixedSize(300,150);
        msgBox.exec();
    }
}

void MainWindow::sendSingleLineCoordinates(int x1, int y1, int x2, int y2)
{
    if (m_tcpCommunicator && m_tcpCommunicator->isConnectedToServer()) {
        m_tcpCommunicator->sendLineCoordinates(x1, y1, x2, y2);
        qDebug() << "기준선 좌표 전송 성공:" << x1 << y1 << x2 << y2;

    } else {
        qDebug() << "TCP 연결이 없어 좌표 전송 실패";
        CustomMessageBox msgBox(nullptr, "전송 실패", "서버에 연결되어 있지 않습니다.");
        msgBox.setFixedSize(300,150);
        msgBox.exec();
    }
}

void MainWindow::onDateChanged(const QDate &date)
{
    qDebug() << "날짜 변경:" << date.toString("yyyy-MM-dd");
}

void MainWindow::onHourChanged(int hour)
{
    if (m_hourSpinBox) {
        m_hourSpinBox->setSuffix(QString("시 ~ %1시").arg(hour + 1));
    }
    qDebug() << "시간 변경:" << QString("%1시~%2시").arg(hour).arg(hour + 1);
}

void MainWindow::onDateButtonClicked()
{
    if (m_calendarDialog) {
        m_calendarWidget->setSelectedDate(m_selectedDate);
        m_calendarDialog->exec();
    }
}

void MainWindow::onCalendarDateSelected(const QDate &date)
{
    m_selectedDate = date;
    m_dateButton->setText(date.toString("yyyy-MM-dd (dddd)"));
    m_calendarDialog->accept();

    qDebug() << "달력에서 날짜 선택:" << date.toString("yyyy-MM-dd");
}

void MainWindow::onHourComboChanged(int index)
{
    int selectedHour = m_hourComboBox->itemData(index).toInt();
    qDebug() << "시간 변경:" << QString("%1시~%2시").arg(selectedHour).arg(selectedHour + 1);
}

void MainWindow::onStreamingButtonClicked()
{
    if (m_videoStreamWidget) {
        if (m_videoStreamWidget->isStreaming()) {
            m_videoStreamWidget->stopStream();
            m_streamingButton->setText("Start Streaming");
            m_streamingButton->setStyleSheet("QPushButton { background-color: #f37321; color: white; padding: 10px 20px; border: none; border-radius: 5px; font-weight: bold; font-size:10pt} "
                                             "QPushButton:hover { background-color: #f37321; }");
        } else {
            if (m_rtspUrl.isEmpty()) {
                CustomMessageBox msgBox(nullptr, "설정 오류", "먼저 네트워크 설정에서 RTSP URL을 설정해주세요.");
                msgBox.setFixedSize(300,150);
                msgBox.exec();
                return;
            }

            m_videoStreamWidget->startStream(m_rtspUrl);
            m_streamingButton->setText("Stop Streaming");
            m_streamingButton->setStyleSheet("QPushButton { background-color: #A5A09E; color: white; padding: 10px 20px; border: none; border-radius: 5px; font-weight: bold; font-size:10pt} "
                                             "QPushButton:hover { background-color: #A5A09E; }");
        }
    }
}

void MainWindow::onRequestImagesClicked()
{
    if (!m_tcpCommunicator || !m_tcpCommunicator->isConnectedToServer()) {
        CustomMessageBox msgBox(nullptr, "연결 오류", "서버에 연결되어 있지 않습니다.\n네트워크 설정을 확인해주세요.");
        msgBox.setFixedSize(300,150);
        msgBox.exec();
        return;
    }

    int selectedHour = m_hourComboBox->currentData().toInt();
    QString dateString = m_selectedDate.toString("yyyy-MM-dd");

    // m_statusLabel->setText("이미지 요청 중... (60초 후 타임아웃)");
    // m_requestButton->setEnabled(false);

    // 타임아웃을 60초로 증가
    // m_requestTimeoutTimer->setInterval(60000);
    // m_requestTimeoutTimer->start();

    // JSON 기반 이미지 요청
    m_tcpCommunicator->requestImageData(dateString, selectedHour);

    qDebug() << QString("JSON 이미지 요청: %1, %2시~%3시").arg(dateString).arg(selectedHour).arg(selectedHour + 1);
}

void MainWindow::onTcpConnected()
{
    m_isConnected = true;
    qDebug() << "TCP 서버 연결 성공 - UI 업데이트";

    if (m_requestButton) {
        m_requestButton->setEnabled(true);
    }


    CustomMessageBox msgBox(nullptr, "연결 성공", "TCP 서버에 성공적으로 연결되었습니다.");
    msgBox.setFixedSize(300,150);
    msgBox.exec();
}

void MainWindow::onTcpDisconnected()
{
    m_isConnected = false;
    qDebug() << "TCP 서버 연결 해제 - UI 업데이트";

    if (m_requestButton) {
        m_requestButton->setEnabled(false);
    }


}

void MainWindow::onTcpError(const QString &error)
{
    qDebug() << "TCP 에러:" << error;

    if (m_requestButton) {
        m_requestButton->setEnabled(false);
    }


    CustomMessageBox msgBox(nullptr, "TCP 연결 오류", error);
    msgBox.setFixedSize(300,150);
    msgBox.exec();
}

void MainWindow::onTcpDataReceived(const QString &data)
{
    qDebug() << "TCP 데이터 수신:" << data;
}

void MainWindow::onTcpPacketReceived(int requestId, int success, const QString &/*data1*/, const QString &/*data2*/, const QString &/*data3*/)
{
    qDebug() << QString("TCP 패킷 수신 - ID: %1, 성공: %2").arg(requestId).arg(success);
}

void MainWindow::onImagesReceived(const QList<ImageData> &images)
{
    qDebug() << QString("이미지 리스트 수신: %1개").arg(images.size());

    if (m_requestTimeoutTimer->isActive()) {
        m_requestTimeoutTimer->stop();
    }

    displayImages(images);

    m_requestButton->setEnabled(true);
}

void MainWindow::onImageClicked(const QString &imagePath, const QString &timestamp, const QString &logText)
{
    QPixmap pixmap;
    if (pixmap.load(imagePath)) {
        m_imageViewerDialog->setImage(pixmap, timestamp, logText);
        m_imageViewerDialog->exec();
    } else {
        CustomMessageBox msgBox(nullptr, "이미지 로드 오류", "이미지를 불러올 수 없습니다.");
        msgBox.setFixedSize(300,150);
        msgBox.exec();
    }
}

void MainWindow::updateLogDisplay()
{
    // 주기적으로 로그 업데이트 (필요시 구현)
}

void MainWindow::onRequestTimeout()
{
    qDebug() << "이미지 요청 타임아웃 (60초)";


    m_requestButton->setEnabled(m_isConnected);

    CustomMessageBox msgBox(nullptr, "요청 타임아웃",
                            "서버에서 60초 내에 응답이 없습니다.\n"
                            "서버 상태와 네트워크 연결을 확인하고 다시 시도해주세요.");
    msgBox.setFixedSize(300,150);
    msgBox.exec();
}

void MainWindow::onStreamError(const QString &error)
{
    qDebug() << "스트림 오류:" << error;
    CustomMessageBox msgBox(nullptr, "스트림 오류", error);
    msgBox.setFixedSize(300,150);
    msgBox.exec();

    if (m_streamingButton) {
        m_streamingButton->setText("Start Streaming");
        m_streamingButton->setStyleSheet("QPushButton { background-color: #f37321; color: white; padding: 10px 20px; border: none; border-radius: 5px; font-weight: bold; font-size:10pt} "
                                         "QPushButton:hover { background-color: #f37321; }");
    }
}

void MainWindow::onCoordinatesConfirmed(bool success, const QString &message)
{
    qDebug() << "좌표 전송 확인 - 성공:" << success << "메시지:" << message;

    if (success) {
        CustomMessageBox msgBox(nullptr, "전송 완료", "좌표가 성공적으로 전송되었습니다.");
        msgBox.setFixedSize(300,150);
        msgBox.exec();
    } else {
        CustomMessageBox msgBox(nullptr, "전송 실패", "좌표 전송에 실패했습니다: " + message);
        msgBox.setFixedSize(300,150);
        msgBox.exec();
    }
}

void MainWindow::onStatusUpdated(const QString &status)
{
    qDebug() << "상태 업데이트:" << status;


}

void MainWindow::sendCategorizedCoordinates(const QList<RoadLineData> &roadLines, const QList<DetectionLineData> &detectionLines)
{
    if (m_tcpCommunicator && m_tcpCommunicator->isConnectedToServer()) {
        // 새로운 RoadLineData 구조를 직접 사용
        if (!roadLines.isEmpty()) {
            bool roadSuccess = m_tcpCommunicator->sendMultipleRoadLines(roadLines);
            if (roadSuccess) {
                qDebug() << "도로선 전송 완료:" << roadLines.size() << "개";
            }
        }

        // 감지선 전송 (기존 방식 유지)
        if (!detectionLines.isEmpty()) {
            bool detectionSuccess = m_tcpCommunicator->sendMultipleDetectionLines(detectionLines);
            if (detectionSuccess) {
                qDebug() << "감지선 전송 완료:" << detectionLines.size() << "개";
            }
        }

        qDebug() << "카테고리별 좌표 전송 완료 - 도로선:" << roadLines.size() << "개, 감지선:" << detectionLines.size() << "개";
    } else {
        qDebug() << "TCP 연결이 없어 좌표 전송 실패";
        CustomMessageBox msgBox(nullptr, "전송 실패", "서버에 연결되어 있지 않습니다.");
        msgBox.setFixedSize(300,150);
        msgBox.exec();
    }
}
