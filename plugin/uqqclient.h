#ifndef UQQCLIENT_H
#define UQQCLIENT_H

#include <QtNetwork>
#include "uqqcontact.h"
#include "uqqgroup.h"

#define TYPE_SEND -1

typedef QMap<QByteArray, QByteArray> RequestHeaderMap;

class UQQClient : public QObject {

    Q_OBJECT
public:
    enum Action {
        // Login phase actions
        CheckCodeAction,
        GetCaptchaAction,
        LoginAction,
        LogoutAction,
        SecondLoginAction,

        // After login
        GetLongNickAction = 10,
        GetMemberAccountAction,
        GetGroupAccountAction,
        GetMemberLevelAction,
        GetMemberInfoAction,
        GetStrangerInfoAction,
        GetUserFaceAction,
        LoadContactAction,
        GetOnlineBuddiesAction,
        PollMessageAction,
        SendBuddyMessageAction,
        SendGroupMessageAction,
        SendSessionMessageAction,
        LoadGroupsAction,
        LoadGroupInfoAction,
        GetGroupSigAction,
        ChangeStatusAction,
        SetGroupMaskAction
    };

    enum Error {
        NoError,
        PasswordError = 3,
        CaptchaError,
        PollNormalReturn = 102,
        PollOfflineError,
        DefaultError = 10000
    };

    //Q_PROPERTY(QVariantMap userInfo READ userInfo NOTIFY userInfoChanged)

    explicit UQQClient(QObject *parent = 0);
    ~UQQClient();

    void addLoginInfo(const QString &key, const QVariant &value);
    Q_INVOKABLE QVariant getLoginInfo(const QString key) const;

    QVariantMap userInfo() const;
    void addUserInfo(const QString &key, const QVariant &value);
    Q_INVOKABLE QVariant getUserInfo(const QString key) const;

    Q_INVOKABLE void checkCode(QString uin);
    Q_INVOKABLE void login(QString uin, QString pwd, QString vc, QString status = "online");
    Q_INVOKABLE void autoReLogin();
    Q_INVOKABLE void logout();
    Q_INVOKABLE void getSimpleInfo(quint64 gid, QString uin);
    Q_INVOKABLE void getMemberDetail(quint64 gid, QString uin);
    Q_INVOKABLE void loadContact();
    Q_INVOKABLE QList<QObject *> getContactList();
    Q_INVOKABLE QList<QObject *> getGroupList();
    Q_INVOKABLE QList<QObject *> getCategoryMembers(quint64 catid);
    Q_INVOKABLE QList<QObject *> getGroupMembers(quint64 gid);
    Q_INVOKABLE QList<QObject *> getMember(QString uin);
    Q_INVOKABLE void getOnlineBuddies();
    Q_INVOKABLE void poll();
    Q_INVOKABLE void sendBuddyMessage(QString dstUin, QString content);
    Q_INVOKABLE void sendGroupMessage(quint64 gid, QString content);
    Q_INVOKABLE void changeStatus(QString status);
    Q_INVOKABLE void loadGroupInfo(quint64 gid);
    Q_INVOKABLE void getGroupSig(quint64 gid, QString dstUin);
    Q_INVOKABLE void sendSessionMessage(quint64 gid, QString dstUin, QString content);
    Q_INVOKABLE void setGroupMask(quint64 gid, int mask);

private:
    void initClient();
    void initConfig();
    QVariant getConfig(const QString &key) const;
    void addConfig(const QString &key, const QVariant &value);

    void get(Action action, QUrl url,
             const QVariantList &attributes = QVariantList(),
             const RequestHeaderMap &headers = RequestHeaderMap());
    void post(Action action, QUrl url,
              const QByteArray &data = QByteArray(),
              const QVariantList &attributes = QVariantList(),
              const RequestHeaderMap &headers = RequestHeaderMap());
    QVariant getResponseResult(const QByteArray &data, int *retCode = Q_NULLPTR);
    void verifyCode(const QString &data);
    void getCaptcha();
    void saveCaptcha(const QByteArray &data);
    void verifyLogin(const QByteArray &data);
    void secondLogin();
    void verifySecondLogin(const QByteArray &data);

    void getMemberFace(const QString &uin);
    void getGroupMemberFace(quint64 gid, const QString &uin);
    void getLongNick(quint64 gid, const QString &uin);
    void parseLongNick(quint64 gid, const QString &uin, const QByteArray &data);
    void getMemberLevel(const QString &uin);
    void parseMemberLevel(const QString &uin, const QByteArray &data);
    void getMemberInfo(const QString &uin);
    void parseMemberInfo(const QString &uin, const QByteArray &data);
    void getStrangerInfo(quint64 gid, const QString &uin);
    void parseStrangerInfo(quint64 gid, const QString &uin, const QByteArray &data);
    void getUserFace();
    void getMemberAccount(quint64 gid, const QString &uin);
    void getGroupAccount(const QString &uin);
    void getAccount(quint64 gid, const QString &uin, Action action);
    void parseAccount(quint64 gid, const QString &uin, const QByteArray &data, Action action);
    void getFace(quint64 gid, const QString &uin, int cache = 0, int type = 1);
    void saveFace(quint64 gid, const QString &uin, const QByteArray &data);
    void parseContact(const QByteArray &data);
    void parseOnlineBuddies(const QByteArray &data);

    void loadGroups();
    void parseGroups(const QByteArray &data);
    void parseGroupInfo(quint64 gid, const QByteArray &data);
    void setMemberDetail(UQQMember *member, const QVariantMap &m);

    QString makeContent(const QString &content);
    QString buddyMessageData(QString dstUin, QString content);
    QString groupMessageData(QString groupUin, QString content);
    QString sessionMessageData(quint64 gid, const QString &dstUin, const QString &content);
    void onMessageSended(quint64 gid, const QString &uin, const QByteArray &data);
    void parseChangeStatus(const QString &status, const QByteArray &data);
    void parseGroupSig(quint64 gid, const QString &dstUin, const QByteArray &data);
    void parseGroupMask(quint64 gid, int mask, const QByteArray &data);

    int parseParamList(const QString &data, QStringList &paramList);
    QString getCookie(const QString &name, QUrl url) const;

    UQQMember *member(quint64 gid, const QString &uin);
    QString getClientId();
    QString getRandom();
    int getRandomInt(int max);
    QString getTimestamp();
    QString imageFormat(const QByteArray &data);

    void onLoginSuccess(const QString &uin, const QString &status);

    void parsePoll(const QByteArray &data);
    void pollStatusChanged(const QVariantMap &m);
    void pollInputNotify(const QVariantMap &m);
    UQQMessage *parseMessage(QString fromUin, const QVariantMap &m);
    void pollMemberMessage(const QVariantMap &m);
    void pollGroupMessage(const QVariantMap &m);
    void pollSessionMessage(const QVariantMap &m);
    void pollKickMessage(const QVariantMap &m);

    void parseLogout(const QByteArray &data);

    void testGetFace(quint64 gid, const QString &uin);

    QString hashFriends(char *uin, char *ptwebqq);
    QByteArray readFile(const QString &filename);

signals:
    void errorChanged(int errCode);
    void captchaChanged(bool needed);
    void loginSuccess();
    void ready();
    void groupReady(quint64 gid);
    void onlineStatusChanged();
    void buddyStatusChanged(quint64 gid, QString uin);

    void pollReceived();
    void memberMessageReceived(quint64 gid);
    void groupMessageReceived(quint64 gid);
    void sessionMessageReceived(quint64 gid);
    void  buddyOnline(QString uin);
    void kicked(QString reason);

public slots:
    void onFinished(QNetworkReply *reply);

private:
    QVariantMap m_loginInfo;
    QVariantMap m_config;
    QNetworkAccessManager *m_manager;

    UQQContact *m_contact;
    UQQGroup *m_group;
};

#endif // UQQCLIENT_H
