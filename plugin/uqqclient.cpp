#include "uqqclient.h"
#include "uqqmemberdetail.h"

//#define UQQ_TEST

#ifdef UQQ_TEST
#define TEST(func) \
    func; return;
#else
#define TEST(func)
#endif

UQQClient::UQQClient(QObject *parent)
    : QObject(parent) {

    m_contact = Q_NULLPTR;
    m_group = Q_NULLPTR;
    m_manager = Q_NULLPTR;

    initClient();

#ifndef UQQ_TEST
    m_manager = new QNetworkAccessManager(this);
    QObject::connect(m_manager, &QNetworkAccessManager::finished,
                    this, &UQQClient::onFinished);
#endif

    addLoginInfo("aid", QVariant("1003903"));  // appid
    addLoginInfo("clientid", getClientId());   // clientid
}

UQQClient::~UQQClient() {
    delete m_contact;
}

void UQQClient::initClient() {
    if (m_contact) {
        delete m_contact;
    }
    m_contact = new UQQContact(this);

    if (m_group) {
        delete m_group;
    }
    m_group = new UQQGroup(this);
}

void UQQClient::initConfig() {
    QString rootPath = QDir::homePath() + "/.UQQ";
    addConfig("rootPath", rootPath);  // the data root path
    QString userPath = rootPath + "/" + getLoginInfo("uin").toString();
    addConfig("userPath", userPath);
    QString groupPath = userPath + "/group";   // the group path
    addConfig("groupPath", groupPath);
    QString groupFacePath = groupPath + "/faces";
    addConfig("groupfacePath", groupFacePath);  // the group face images path
    QString facePath = userPath + "/faces";
    addConfig("facePath", facePath);    // the face images path

    QDir path;
    if (!path.mkpath(facePath) || !path.mkpath(groupPath) || !path.mkpath(groupFacePath))
        qCritical() << "Error: make path";
}

void UQQClient::onFinished(QNetworkReply *reply) {
    bool ok;
    Action action = (Action)reply->request().attribute(QNetworkRequest::User).toInt(&ok);
    QVariantList p = reply->request().attribute(QNetworkRequest::UserMax).toList();

    if (!ok || reply->error() != QNetworkReply::NoError) {
        qWarning() << action << reply->error() << reply->errorString();
        return;
    }

    QByteArray data = reply->readAll();
    switch (action) {
    case CheckCodeAction:
        verifyCode(data);
        break;
    case GetCaptchaAction:
        saveCaptcha(data);
        break;
    case LoginAction:
        verifyLogin(data);
        break;
    case LogoutAction:
        parseLogout(data);
        break;
    case SecondLoginAction:
        verifySecondLogin(data);
        break;
    case GetMemberAccountAction:
        parseAccount(p.at(0).toULongLong(), p.at(1).toString(), data, GetMemberAccountAction);
        break;
    case GetGroupAccountAction:
        parseAccount(p.at(0).toULongLong(), p.at(1).toString(), data, GetGroupAccountAction);
        break;
    case GetLongNickAction:
        parseLongNick(p.at(0).toULongLong(), p.at(1).toString(), data);
        break;
    case GetMemberLevelAction:
        parseMemberLevel(p.at(0).toString(), data);
        break;
    case GetMemberInfoAction:
        parseMemberInfo(p.at(0).toString(), data);
        break;
    case GetStrangerInfoAction:
        parseStrangerInfo(p.at(0).toULongLong(), p.at(1).toString(), data);
        break;
    case GetUserFaceAction:
        saveFace(p.at(0).toULongLong(), p.at(1).toString(), data);
        break;
    case LoadContactAction:
        parseContact(data);
        break;
    case GetOnlineBuddiesAction:
        parseOnlineBuddies(data);
        break;
    case PollMessageAction:
        parsePoll(data);
        break;
    case SendBuddyMessageAction:
        onMessageSended(p.at(0).toULongLong(), p.at(1).toString(), data);
        break;
    case SendGroupMessageAction:
        onMessageSended(p.at(0).toULongLong(), p.at(1).toString(), data);
        break;
    case SendSessionMessageAction:
        onMessageSended(p.at(0).toULongLong(), p.at(1).toString(), data);
        break;
    case ChangeStatusAction:
        parseChangeStatus(p.at(0).toString(), data);
        break;
    case LoadGroupsAction:
        parseGroups(data);
        break;
    case LoadGroupInfoAction:
        parseGroupInfo(p.at(0).toULongLong(), data);
        break;
    case GetGroupSigAction:
        parseGroupSig(p.at(0).toULongLong(),
                      p.at(1).toString(),
                      data);
        break;
    case SetGroupMaskAction:
        parseGroupMask(p.at(0).toULongLong(),
                       UQQCategory::GroupMessageMask(p.at(1).toInt()),
                       data);
        break;
    default:
        qWarning() << "Unknown action:" << action;
    }

    reply->deleteLater();
}

void UQQClient::get(Action action, QUrl url,
                    const QVariantList &attributes,
                    const RequestHeaderMap &headers) {
    QNetworkRequest request;

    request.setUrl(url);
    request.setAttribute(QNetworkRequest::User, action);
    request.setAttribute(QNetworkRequest::UserMax, attributes);

    request.setRawHeader("Referer", "http://s.web2.qq.com/proxy.html?v=20110412001&callback=1&id=1");

    for (RequestHeaderMap::ConstIterator iter = headers.constBegin(); iter != headers.constEnd(); iter++)
        request.setRawHeader(iter.key(), iter.value());

    m_manager->get(request);
}

void UQQClient::post(Action action, QUrl url,
                     const QByteArray &data,
                     const QVariantList &attributes,
                     const RequestHeaderMap &headers) {
    QNetworkRequest request;

    request.setUrl(url);
    request.setAttribute(QNetworkRequest::User, action);
    request.setAttribute(QNetworkRequest::UserMax, attributes);

    request.setRawHeader("Referer", "http://s.web2.qq.com/proxy.html?v=20110412001&callback=1&id=1");
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");

    for (RequestHeaderMap::ConstIterator iter = headers.constBegin(); iter != headers.constEnd(); iter++)
        request.setRawHeader(iter.key(), iter.value());

    m_manager->post(request, data);
}

QVariant UQQClient::getResponseResult(const QByteArray &data, int *retCode) {
    QJsonObject obj = QJsonDocument::fromJson(data).object();
    QVariantMap m = obj.toVariantMap();

    if (retCode)
        *retCode = m.value("retcode", DefaultError).toInt();

    return m.value("result");
}

void UQQClient::checkCode(QString uin) {
    qDebug() << "check code...";

    QUrl url("http://check.ptlogin2.qq.com/check");
    QUrlQuery query;
    query.addQueryItem("uin", uin);
    query.addQueryItem("appid", getLoginInfo("aid").toString());
    query.addQueryItem("r", getRandom());
    url.setQuery(query);
    qDebug() << url.toString();

    addLoginInfo("uin", uin);

    TEST(verifyCode(readFile("test/verifycode.txt")));
    get(CheckCodeAction, url);
}

void UQQClient::verifyCode(const QString &data) {
    QStringList list;
    parseParamList(data, list);

    addLoginInfo("vc", list.at(1));
    addLoginInfo("uinHex", list.at(2).toUtf8());
    if (list.at(0).toInt() != NoError) {
        if (list.at(1).length() > 0) {
            qDebug() << "check code done, code needed.";
            getCaptcha();
        }
        else {
           qWarning() << data;
        }
    } else {
        qDebug() << "check code done, code no needed.";
        emit captchaChanged(false);
    }
}

void UQQClient::getCaptcha() {
    qDebug() << "get captcha...";

    QUrl url("http://captcha.qq.com/getimage");
    QUrlQuery query;
    query.addQueryItem("uin", getLoginInfo("uin").toString());
    query.addQueryItem("aid", getLoginInfo("aid").toString());
    query.addQueryItem("r", getRandom());
    url.setQuery(query);
    qDebug() << url.toString();

    get(GetCaptchaAction, url);
}

void UQQClient::saveCaptcha(const QByteArray &data) {
    QString capName("captcha" + imageFormat(data));
    QFile file(capName);

    file.open(QIODevice::WriteOnly | QIODevice::Truncate);
    file.write(data);
    file.close();
    qDebug() << "get captcha done, captcha saved.";
    addLoginInfo("captcha", capName);
    emit captchaChanged(true);
}

void UQQClient::logout() {
    QUrl url("http://s.web2.qq.com/channel/logout2");
    QUrlQuery query;
    query.addQueryItem("ids", "");
    query.addQueryItem("clientid", getLoginInfo("clientid").toString());
    query.addQueryItem("psessionid", getLoginInfo("psessionid").toString());
    query.addQueryItem("t", getTimestamp());
    url.setQuery(query);
    qDebug() << url.toString();

    get(LogoutAction, url);
}

void UQQClient::parseLogout(const QByteArray &data) {
    QJsonObject obj = QJsonDocument::fromJson(data).object();
    QVariantMap m = obj.toVariantMap();

    int retcode = m.value("retcode", DefaultError).toInt();
    if (retcode == NoError) {
        qDebug() << "logout ok";
    } else {
        qWarning() << "parseLogout:" << data;
    }
}

void UQQClient::login(QString uin, QString pwd, QString vc, QString status) {
    qDebug() << "request login...";

    QUrl url("http://ptlogin2.qq.com/login");
    //QUrl u1("http://web.qq.com/loginproxy.html?login2qq=1&webqq_type=10");
    QUrlQuery query;
    query.addQueryItem("u", uin);
    query.addQueryItem("p", pwd);
    query.addQueryItem("verifycode", vc);
    query.addQueryItem("aid", getLoginInfo("aid").toString());
    query.addQueryItem("webqq_type", QString::number(10));
    query.addQueryItem("remember_uin", QString::number(0));
    query.addQueryItem("login2qq", QString::number(1));
    //query.addQueryItem("u1", u1.toEncoded());
    query.addQueryItem("h", QString::number(1));
    query.addQueryItem("ptredirect", QString::number(0));
    query.addQueryItem("ptlang", QString::number(2052));
    query.addQueryItem("from_ui", QString::number(1));
    query.addQueryItem("pttype", QString::number(1));
    query.addQueryItem("fp", "loginerroralert");
    query.addQueryItem("action", "2-6-22950");
    query.addQueryItem("mibao_css", "m_webqq");
    query.addQueryItem("t", QString::number(1));
    query.addQueryItem("g", QString::number(1));
    url.setQuery(query);
    url = QUrl(url.toString().append("&u1=http%3A%2F%2Fweb.qq.com%2Floginproxy.html%3Flogin2qq%3D1%26webqq_type%3D10"));
    qDebug() << url.toString();

    addLoginInfo("uin", uin);
    addLoginInfo("vc", vc);
    addLoginInfo("pwd", pwd);
    addLoginInfo("status", status);

    TEST(verifyLogin(readFile("test/retok.txt")));
    get(LoginAction, url);
}

void UQQClient::verifyLogin(const QByteArray &data) {
    QStringList list;
    int errCode;
    parseParamList(data, list);

    if ((errCode = list.at(0).toInt()) == NoError) {
        qDebug() << "login done.";
        secondLogin();
    } else {
        qWarning() << data;
        addLoginInfo("errMsg", list.at(4));
        if (errCode == CaptchaError) {   // get captcha again
            getCaptcha();
        }
    }
    emit errorChanged(errCode);
}

void UQQClient::autoReLogin() {
    secondLogin();
}

void UQQClient::secondLogin() {
    qDebug() << "request second login...";

    QUrl url("http://d.web2.qq.com/channel/login2");
    qDebug() << url.toString();

    QString ptwebqq = getCookie("ptwebqq", url);
    addLoginInfo("ptwebqq", ptwebqq);
    qDebug() << "ptwebqq:" << ptwebqq;

    QVariantMap param;
    param.insert("status", getLoginInfo("status").toString());
    param.insert("ptwebqq", ptwebqq);
    param.insert("passwd_sig", "");
    param.insert("clientid", getLoginInfo("clientid").toString());
    param.insert("psessionid", getLoginInfo("psessionid").toString());
    QJsonDocument doc;
    doc.setObject(QJsonObject::fromVariantMap(param));
    QString p = "r=" + doc.toJson();
    p.append(QString("&clientid=%1&psessionid=%2")
             .arg(getLoginInfo("clientid").toString(), getLoginInfo("psessionid").toString()));

    TEST(verifySecondLogin(readFile("test/loginSuccess.txt")));
    post(SecondLoginAction, url, QUrl::toPercentEncoding(p, "=&"));
}

void UQQClient::verifySecondLogin(const QByteArray &data) {
    qDebug() << "verify second login...";
    const QVariantMap &result = getResponseResult(data).toMap();

    if (!result.isEmpty()) {
        addLoginInfo("uin", result.value("uin").toString());
        addLoginInfo("cip", result.value("cip").toUInt());
        addLoginInfo("index", result.value("index").toInt());
        addLoginInfo("port", result.value("port").toInt());
        addLoginInfo("status", result.value("status").toString());
        addLoginInfo("vfwebqq", result.value("vfwebqq"));
        addLoginInfo("psessionid", result.value("psessionid"));

        qDebug() << "second login done.";

        onLoginSuccess(getLoginInfo("uin").toString(),
                       result.value("status").toString());
    }
}

void UQQClient::onLoginSuccess(const QString &uin, const QString &status) {
    initClient();
    initConfig();

    UQQMember *user = new UQQMember(UQQCategory::IllegalCategoryId, uin, m_contact);
    qDebug() << "login success! status:" << status;
    user->setStatus(UQQMember::statusIndex(status));
    m_contact->addMember(user);

    qDebug() << "get user" << uin << "information";
    getUserFace();
    getLongNick(UQQCategory::IllegalCategoryId, uin);
    getMemberDetail(UQQCategory::IllegalCategoryId, uin);

    loadContact();
}

void UQQClient::getSimpleInfo(quint64 gid, QString uin) {
    getFace(gid, uin);

    UQQMember *member = this->member(gid, uin);
    if (q_check_ptr(member) && member->isFriend())
        getLongNick(gid, uin);
}

void UQQClient::getMemberDetail(quint64 gid, QString uin) {
    UQQMember *member = this->member(gid, uin);

    if (!q_check_ptr(member)) return;

    if (member->isFriend()) {
        getMemberInfo(uin);
        getMemberLevel(uin);
    } else {
        getStrangerInfo(gid, uin);
        if (member->groupSig().isEmpty())
            getGroupSig(gid, uin);
    }
    getMemberAccount(gid, uin);

}

void UQQClient::getMemberAccount(quint64 gid, const QString &uin) {
    getAccount(gid, uin, GetMemberAccountAction);
}

void UQQClient::getGroupAccount(const QString &uin) {
    qDebug() << "request group account...";
    getAccount(UQQCategory::IllegalCategoryId, uin, GetGroupAccountAction);
}

void UQQClient::getAccount(quint64 gid, const QString &uin, Action action) {
    qDebug() << "get account...";
    QUrl url("http://s.web2.qq.com/api/get_friend_uin2");
    QUrlQuery query;
    query.addQueryItem("tuin", uin);
    query.addQueryItem("verifysession", "");
    query.addQueryItem("type", QString::number(1));
    query.addQueryItem("code", "");
    query.addQueryItem("vfwebqq", getLoginInfo("vfwebqq").toString());
    query.addQueryItem("t", getTimestamp());
    url.setQuery(query);
    qDebug() << url.toString();

    QVariantList attributes;
    attributes << gid << uin;

    TEST(parseAccount(gid, uin, readFile("test/account.txt"), action));
    get(action, url, attributes);
}

/*
 *{"retcode":0,"result":{"uiuin":"","account":123456,"uin":12345668}}
 */
void UQQClient::parseAccount(quint64 gid, const QString &uin, const QByteArray &data, Action action) {
    const QVariantMap &result = getResponseResult(data).toMap();

    if (!result.isEmpty()) {
        if (action == GetMemberAccountAction) {
            UQQMember *member = this->member(gid, uin);
            if (!q_check_ptr(member)) return;

            UQQMemberDetail *detail = Q_NULLPTR;
            if ((detail = member->detail()) == Q_NULLPTR) {
                detail = new UQQMemberDetail(member);
                member->setDetail(detail);
            }
            member->detail()->setAccount(result.value("account").toULongLong());
            qDebug() << "get account done." << member->detail()->account();
        } else {
            UQQCategory *group = m_group->getGroupByCode(uin.toULongLong());

            if (q_check_ptr(group)) {
                group->setAccount(result.value("account").toULongLong());
                qDebug() << "request group account done, group" << gid << "account:" << group->account();
            }
        }
    }
}

void UQQClient::getLongNick(quint64 gid, const QString &uin) {
    QUrl url("http://s.web2.qq.com/api/get_single_long_nick2");
    QUrlQuery query;
    query.addQueryItem("tuin", uin);
    query.addQueryItem("vfwebqq", getLoginInfo("vfwebqq").toString());
    query.addQueryItem("t", getTimestamp());
    url.setQuery(query);
    //qDebug() << url.toString();

    QVariantList attributes;
    attributes << gid << uin;

    TEST(parseLongNick(gid, uin, readFile("test/lnick.txt")));
    get(GetLongNickAction, url, attributes);
}

/*
 * parseLongNick()
 * long nick json format:
 * {"retcode":0,"result":[{"uin":1279450562,"lnick":"123456"}]}
 */
void UQQClient::parseLongNick(quint64 gid, const QString &uin, const QByteArray &data) {
    const QVariantList &result = getResponseResult(data).toList();

    UQQMember *member = Q_NULLPTR;

    if (!result.isEmpty()) {
        const QVariantMap &m = result.at(0).toMap();
        member = this->member(gid, uin);

        if (q_check_ptr(member))
            member->setLongnick(m.value("lnick").toString());
    }
}

void UQQClient::getMemberLevel(const QString &uin) {
    QUrl url("http://s.web2.qq.com/api/get_qq_level2");
    QUrlQuery query;
    query.addQueryItem("tuin", uin);
    query.addQueryItem("vfwebqq", getLoginInfo("vfwebqq").toString());
    url.setQuery(query);
    qDebug() << url.toString();

    QVariantList attributes;
    attributes << uin;

    TEST(parseMemberLevel(uin, readFile("test/level.txt")));
    get(GetMemberLevelAction, url, attributes);
}

/*
{
    "retcode":0,
    "result":{"level":40,"days":1802,"hours":13476,"remainDays":43,"tuin":121830387}
}
*/
void UQQClient::parseMemberLevel(const QString &uin, const QByteArray &data) {
    UQQMember *member = this->member(UQQCategory::IllegalCategoryId, uin);
    UQQMemberDetail *detail = Q_NULLPTR;
    const QVariantMap &result = getResponseResult(data).toMap();

    if (!result.isEmpty() && q_check_ptr(member)) {
        if ((detail = member->detail()) == Q_NULLPTR) {
            detail = new UQQMemberDetail(member);
            member->setDetail(detail);
        }
        member->detail()->setLevel(result.value("level").toInt());
        member->detail()->setLevelDays(result.value("days").toInt());
        member->detail()->setLevelHours(result.value("hours").toInt());
        member->detail()->setLevelRemainDays(result.value("remainDays").toInt());
    }
}

void UQQClient::getMemberInfo(const QString &uin) {
    QUrl url("http://s.web2.qq.com/api/get_friend_info2");
    QUrlQuery query;
    query.addQueryItem("tuin", uin);
    query.addQueryItem("vfwebqq", getLoginInfo("vfwebqq").toString());
    query.addQueryItem("t", getTimestamp());
    url.setQuery(query);
    qDebug() << url.toString();

    QVariantList attributes;
    attributes << uin;

    TEST(parseMemberInfo(uin, readFile("test/user.txt")))
    get(GetMemberInfoAction, url, attributes);
}

/*
{
"retcode":0,
"result":{
    "face":0,"birthday":{"month":3,"year":2013,"day":25},"occupation":"",
    "phone":"123456","allow":1,"college":"","uin":123456,"constel":1,"blood":1,
    "homepage":"","stat":0,"vip_info":0,"country":"","city":"","personal":"",
    "nick":"","shengxiao":1,"email":"","province":"","gender":"","mobile":""}
}
*/
void UQQClient::parseMemberInfo(const QString &uin, const QByteArray &data) {
    const QVariantMap &result = getResponseResult(data).toMap();
    UQQMember *member = Q_NULLPTR;

    if (!result.isEmpty()) {
        member = this->member(UQQCategory::IllegalCategoryId, uin);
        if (q_check_ptr(member)) setMemberDetail(member, result);
    }
}

void UQQClient::getStrangerInfo(quint64 gid, const QString &uin) {
    qDebug() << "get stranger info...";
    QUrl url("http://s.web2.qq.com/api/get_stranger_info2");
    QUrlQuery query;
    query.addQueryItem("tuin", uin);
    query.addQueryItem("vfwebqq", getLoginInfo("vfwebqq").toString());
    query.addQueryItem("t", getTimestamp());
    url.setQuery(query);
    qDebug() << url.toString();

    QVariantList attributes;
    attributes << gid << uin;

    TEST(parseStrangerInfo(gid, uin, readFile("test/user.txt")));
    get(GetStrangerInfoAction, url, attributes);
}

void UQQClient::parseStrangerInfo(quint64 gid, const QString &uin, const QByteArray &data) {
    const QVariantMap &result = getResponseResult(data).toMap();
    UQQMember *member = Q_NULLPTR;

    if (!result.isEmpty()) {
        if ((member = this->member(gid, uin)) == Q_NULLPTR) {
            member = new UQQMember(gid, uin, m_contact);
            QList<UQQMessage *> messages = m_contact->getSessMessage(uin);
            UQQMessage *message;
            foreach(message, messages) {
                message->setParent(member);
                message->setName(member->card() == "" ? member->nickname() : member->card());
                member->addMessage(message);
            }

            if (messages.size() > 0)
                qDebug() << "stranger has messages:" << messages.size();

            m_contact->addMember(member);
        }
        qDebug() << "get stranger info done.";
        setMemberDetail(member, result);
    }
}

void UQQClient::setMemberDetail(UQQMember *member, const QVariantMap &m) {
    UQQMemberDetail *detail = Q_NULLPTR;
    qDebug() << "set member detail...";
    member->setNickname(m.value("nick").toString());
    //member->setStatus(m.value("stat").toInt() / 10);

    if ((detail = member->detail()) == Q_NULLPTR)
        detail = new UQQMemberDetail(member);

    detail->setFaceid(m.value("face").toInt());
    detail->setOccupation(m.value("occupation").toString());
    detail->setPhone(m.value("phone").toString());
    detail->setAllow(m.value("allow").toBool());
    detail->setCollege(m.value("college").toString());
    detail->setConstel(m.value("constel").toInt());
    detail->setBlood(m.value("blood").toInt());
    detail->setHomepage(m.value("homepage").toString());
    detail->setCountry(m.value("country").toString());
    detail->setCity(m.value("city").toString());
    detail->setPersonal(m.value("personal").toString());
    member->setNickname(m.value("nick").toString());    // nickname not in detail
    detail->setShengxiao(m.value("shengxiao").toInt());
    detail->setEmail(m.value("email").toString());
    detail->setProvince(m.value("province").toString());
    detail->setGender(UQQMemberDetail::genderIndex(m.value("gender").toString()));
    detail->setMobile(m.value("mobile").toString());
    detail->setToken(m.value("token").toString());

    QVariantMap birth = m.value("birthday").toMap();
    int year = birth.value("year").toInt();
    int month = birth.value("month").toInt();
    int day = birth.value("day").toInt();
    if (QDate::isValid(year, month, day)) {
        QDateTime birth;
        birth.setDate(QDate(year, month, day));
        detail->setBirthday(birth);
    }

    member->setDetail(detail);
    qDebug() << "set member detail done.";
}

UQQMember *UQQClient::member(quint64 gid, const QString &uin) {
    UQQMember *member = Q_NULLPTR;
    UQQCategory *cat = Q_NULLPTR;

    if ((member = m_contact->member(uin)) == Q_NULLPTR && gid != UQQCategory::IllegalCategoryId) {
        if ((cat = m_group->getGroupById(gid)) != Q_NULLPTR) {
            member = cat->member(uin);
        }
    }
    return member;
}

void UQQClient::getUserFace() {
    getFace(UQQCategory::IllegalCategoryId, getLoginInfo("uin").toString(), 1);
}

void UQQClient::testGetFace(quint64 gid, const QString &uin) {
    QString path = "../121830387.bmp";
    UQQMember *member = this->member(gid, uin);
    if (q_check_ptr(member)) member->setFace(path);
}

void UQQClient::getFace(quint64 gid, const QString &uin, int cache, int type) {
    QUrl url(QString("http://face%1.qun.qq.com/cgi/svr/face/getface").arg(QString::number(qrand() % 10 + 1)));
    QUrlQuery query;
    query.addQueryItem("cache", QString::number(cache));
    query.addQueryItem("type", QString::number(type));
    query.addQueryItem("uin", uin);
    query.addQueryItem("vfwebqq", getLoginInfo("vfwebqq").toString());
    url.setQuery(query);

    QVariantList attributes;
    attributes << gid << uin;

    TEST(testGetFace(gid, uin));
    get(GetUserFaceAction, url, attributes);
}

void UQQClient::saveFace(quint64 gid, const QString &uin, const QByteArray &data) {
    QString facePath = getConfig("facePath").toString();
    UQQMember *member = this->member(gid, uin);

    if (!q_check_ptr(member)) return;
    QString path = facePath + "/" + uin + imageFormat(data);

    QFile file(path);
    file.open(QIODevice::WriteOnly | QIODevice::Truncate);
    file.write(data);
    file.close();

    member->setFace(path);
}

void UQQClient::changeStatus(QString status) {
    QUrl url("http://d.web2.qq.com/channel/change_status2");
    QUrlQuery query;
    query.addQueryItem("newstatus", status);
    query.addQueryItem("clientid", getLoginInfo("clientid").toString());
    query.addQueryItem("psessionid", getLoginInfo("psessionid").toString());
    query.addQueryItem("t", getTimestamp());
    url.setQuery(query);
    qDebug() << url.toString();

    QVariantList attributes;
    attributes << status;

    TEST(parseChangeStatus(status, readFile("test/retok.txt")));
    get(ChangeStatusAction, url, attributes);
}

void UQQClient::parseChangeStatus(const QString &status, const QByteArray &data) {
    int retCode = 0;
    getResponseResult(data, &retCode);

    if (retCode == NoError) {
        qDebug() << "change status ok:" << status;
        QString uin = getLoginInfo("uin").toString();
        UQQMember *user = this->member(UQQCategory::IllegalCategoryId, uin);
        if (q_check_ptr(user)) {
            user->setStatus(UQQMember::statusIndex(status));
            addLoginInfo("status", status);
        }
    }
}

QString UQQClient::hashFriends(char *uin, char *ptwebqq) {
    size_t len = strlen(ptwebqq)+strlen("password error")+3;
    char* a = (char *)malloc(len);
    memset(a, 0, len);
    const char* b = uin;
    strcat(strcpy(a,ptwebqq),"password error");
    size_t alen = strlen(a);
    len = 2048;
    char* s = (char *)malloc(len);
    memset(s, 0, len);
    int *j = (int *)malloc(sizeof(int)*alen);
    for(;;){
        if(strlen(s)<=alen){
            if(strcat(s,b),strlen(s)==alen) break;
        }else{
            s[alen]='\0';
            break;
        }
    }
    size_t d;
    for(d=0;d<strlen(s);d++){
        j[d]=s[d]^a[d];
    }
    const char* ch = "0123456789ABCDEF";
    s[0]=0;
    for(d=0;d<alen;d++){
        s[2*d]=ch[j[d]>>4&15];
        s[2*d+1]=ch[j[d]&15];
    }
    free(a);
    free(j);
    QString result(s);
    free(s);
    return result;
}

void UQQClient::loadContact() {
    qDebug() << "request contact list...";
    QVariantMap param;
    QUrl url("http://s.web2.qq.com/api/get_user_friends2");

    param.insert("h", "hello");
    param.insert("hash", hashFriends(getLoginInfo("uin").toString().toLatin1().data(), getLoginInfo("ptwebqq").toString().toLatin1().data()));
    param.insert("vfwebqq", getLoginInfo("vfwebqq"));
    QJsonDocument doc;
    doc.setObject(QJsonObject::fromVariantMap(param));
    QString p = "r=" + doc.toJson();

    TEST(parseContact(readFile("test/friends.txt")));
    post(LoadContactAction, url, QUrl::toPercentEncoding(p, "=&"));
}

void UQQClient::parseContact(const QByteArray &data) {
    int retCode = NoError;
    const QVariantMap &result = getResponseResult(data, &retCode).toMap();

    if (retCode == NoError) {
        if (!result.isEmpty())
            m_contact->setContactData(result);
        qDebug() << "contact list ready.";
        getOnlineBuddies();
    }
}

void UQQClient::getOnlineBuddies() {
    qDebug() << "request online buddies...";
    QUrl url("http://d.web2.qq.com/channel/get_online_buddies2");
    QUrlQuery query;
    query.addQueryItem("clientid", getLoginInfo("clientid").toString());
    query.addQueryItem("psessionid", getLoginInfo("psessionid").toString());
    query.addQueryItem("t", getTimestamp());
    url.setQuery(query);
    qDebug() << url.toString();

    TEST(parseOnlineBuddies(readFile("test/status.txt")));
    get(GetOnlineBuddiesAction, url);
}

/*
 * {"retcode":0,"result":[{"uin":1234567,"status":"online","client_type":1},...,{}]}
 */
void UQQClient::parseOnlineBuddies(const QByteArray &data) {
    int retCode = NoError;
    const QVariantList &result = getResponseResult(data, &retCode).toList();

    if (retCode == NoError) {
        if (!result.isEmpty())
            m_contact->setOnlineBuddies(result);
        qDebug() << "request online buddies done.";
        loadGroups();
    }
}

void UQQClient::loadGroups() {
    qDebug() << "request group list...";
    QUrl url("http://s.web2.qq.com/api/get_group_name_list_mask2");

    QVariantMap param;
    QJsonDocument doc;
    param.insert("vfwebqq", getLoginInfo("vfwebqq"));
    doc.setObject(QJsonObject::fromVariantMap(param));
    QString p = "r=" + doc.toJson();

    TEST(parseGroups(readFile("test/group.txt")));
    post(LoadGroupsAction, url, QUrl::toPercentEncoding(p, "=&"));
}

void UQQClient::parseGroups(const QByteArray &data) {
    int retCode = NoError;
    const QVariantMap &result = getResponseResult(data, &retCode).toMap();

    if (retCode == NoError) {
        if (!result.isEmpty())
            m_group->setGroupData(result);
        qDebug() << "request group list done.";
        qDebug() << "ALL needed datas are loaded, now show the main page.";
        emit ready();
    }
}

void UQQClient::loadGroupInfo(quint64 gid) {
    qDebug() << "request group info..." << gid;
    UQQCategory *group = m_group->getGroupById(gid);
    if (!q_check_ptr(group)) return;

    QUrl url("http://s.web2.qq.com/api/get_group_info_ext2");
    QUrlQuery query;
    query.addQueryItem("gcode", QString::number(group->code()));
    query.addQueryItem("vfwebqq", getLoginInfo("vfwebqq").toString());
    query.addQueryItem("t", getTimestamp());
    url.setQuery(query);
    qDebug() << url.toString();

    QVariantList attributes;
    attributes << gid;

    TEST(parseGroupInfo(gid, readFile("test/groupinfo02.txt")));
    get(LoadGroupInfoAction, url, attributes);
}

void UQQClient::parseGroupInfo(quint64 gid, const QByteArray &data) {
    int retCode = NoError;
    const QVariantMap &result = getResponseResult(data, &retCode).toMap();

    if (retCode == NoError) {
        if (!result.isEmpty())
            m_group->setGroupDetail(gid, result, m_contact);
        emit groupReady(gid);

        qDebug() << "request group" << gid << "info done.";

        UQQCategory *group = m_group->getGroupById(gid);
        if (!q_check_ptr(group)) return;

        getGroupAccount(QString::number(group->code()));
    }
}

void UQQClient::setGroupMask(quint64 gid, int mask) {
    QUrl url("http://cgi.web2.qq.com/keycgi/qqweb/uac/messagefilter.do");
    QUrlQuery query;
    query.addQueryItem("retype", QString::number(1));
    query.addQueryItem("app", "EQQ");
    query.addQueryItem("vfwebqq", getLoginInfo("vfwebqq").toString());

    QJsonDocument doc;
    QVariantMap itemlist;
    QVariantMap groupmask;
    groupmask.insert(QString::number(gid), QString::number(mask));
    groupmask.insert("cAll", QString::number(0));
    groupmask.insert("idx", getLoginInfo("idx"));
    groupmask.insert("port", getLoginInfo("port"));
    itemlist.insert("groupmask", groupmask);

    doc.setObject(QJsonObject::fromVariantMap(itemlist));
    query.addQueryItem("itemlist", doc.toJson());
    const QString &p = query.toString(QUrl::FullyDecoded);

    qDebug() << url.toString();
    //qDebug() << p;

    QVariantList attributes;
    attributes << gid << mask;

    TEST(parseGroupMask(gid, mask, readFile("test/retok.txt")));
    post(SetGroupMaskAction, url, QUrl::toPercentEncoding(p, "=&"), attributes);
}

void UQQClient::parseGroupMask(quint64 gid,
                                 int mask,
                                 const QByteArray &data) {
    UQQCategory *group;
    int retCode = NoError;
    getResponseResult(data, &retCode).toMap();

    if (retCode == NoError) {
        group = m_group->getGroupById(gid);
        group->setMessageMask(UQQCategory::GroupMessageMask(mask));
    }
}

/*
 * "content":"[\"hello\",[\"face\",1],\"world\",
 * [\"font\",{\"name\":\"宋体\",\"size\":\"10\",\"style\":[0,0,0],\"color\":\"000000\"}]]"
 */
QString UQQClient::makeContent(const QString &content) {
    QString result = "[";
    QString msg(content);
    QRegExp facePattern("\\[face\\d{1,3}\\]");
    QString faceContent;
    int pos = 0;
    QString fontContent = QString("[\"font\",{\"name\":\"Arial\",\"size\":\"10\",\"style\":[0,0,0],\"color\":\"000000\"}]");

    while (!msg.isEmpty()) {
        pos = msg.indexOf(facePattern);
        if (pos < 0) {
            result.append("\"").append(msg).append("\",");
            msg.clear();
        } else {
            if (pos > 0) {      // append the text before the face
                result.append("\"").append(msg.left(pos)).append("\",");
                msg.remove(0, pos); // discard the text before the face
            }
            pos = msg.indexOf(']');
            if (pos < 0) qWarning() << "face content unnormal.";
            Q_ASSERT(pos > 0);
            faceContent = msg.left(pos);    // exclude the last ']' character
            result.append("[\"face\",").append(faceContent.mid(5)).append("],"); // get the face id;
            msg.remove(0, pos + 1); // remove the face content
        }
    }

    result.append(fontContent).append("]");
    //qDebug() << result;

    return result;
}

QString UQQClient::buddyMessageData(QString dstUin, QString content) {
    UQQMember *user = this->member(UQQCategory::IllegalCategoryId, getLoginInfo("uin").toString());
    if (!q_check_ptr(user)) return "";

    QJsonObject obj;
    obj.insert("to", dstUin);
    obj.insert("face", QString::number(user->detail()->faceid()));
    obj.insert("msg_id", QString::number(getRandomInt(10000000)));
    obj.insert("clientid", getLoginInfo("clientid").toString());
    obj.insert("psessionid", getLoginInfo("psessionid").toString());

    obj.insert("content", makeContent(content));

    QJsonDocument doc(obj);
    return doc.toJson();
}

QString UQQClient::groupMessageData(QString groupUin, QString content) {
    QJsonObject obj;
    obj.insert("group_uin", groupUin);
    obj.insert("msg_id", QString::number(getRandomInt(10000000)));
    obj.insert("clientid", getLoginInfo("clientid").toString());
    obj.insert("psessionid", getLoginInfo("psessionid").toString());

    obj.insert("content", makeContent(content));

    QJsonDocument doc(obj);
    return doc.toJson();
}

QString UQQClient::sessionMessageData(quint64 gid, const QString &dstUin, const QString &content) {
    UQQMember *user = this->member(UQQCategory::IllegalCategoryId, getLoginInfo("uin").toString());
    if (!q_check_ptr(user)) return "";
    UQQMember *member = this->member(gid, dstUin);
    if (!q_check_ptr(member)) return "";

    QJsonObject obj;
    obj.insert("to", dstUin);
    obj.insert("group_sig", member->groupSig());
    obj.insert("face", QString::number(user->detail()->faceid()));
    obj.insert("msg_id", QString::number(getRandomInt(10000000)));
    obj.insert("clientid", getLoginInfo("clientid").toString());
    obj.insert("psessionid", getLoginInfo("psessionid").toString());

    obj.insert("content", makeContent(content));

    QJsonDocument doc(obj);
    return doc.toJson();
}

/*
r={
    "to":123456,
    "face":123,
    "content":"["hello",["font",{"name":"Arial","size":"10","style":[0,0,0],"color":"000000"}]]",
    "msg_id":123456,
    "clientid":"123456",
    "psessionid":"..."
  }&clientid=123456&psessionid=....
*/
void UQQClient::sendBuddyMessage(QString dstUin, QString content) {
    QUrl url("http://d.web2.qq.com/channel/send_buddy_msg2");
    QString p = "r=" + buddyMessageData(dstUin, content);
    p.append(QString("&clientid=%1&psessionid=%2").arg(getLoginInfo("clientid").toString(), getLoginInfo("psessionid").toString()));

    QString fromUin = getLoginInfo("uin").toString();
    UQQMember *member = this->member(UQQCategory::IllegalCategoryId, dstUin);
    if (!q_check_ptr(member)) return;

    UQQMessage *message = new UQQMessage(member);
    message->setType(TYPE_SEND);
    message->setSrc(fromUin);
    message->setDst(dstUin);
    message->setContent(content);
    message->setTime(QDateTime::currentDateTime());

    UQQMember *user = this->member(UQQCategory::IllegalCategoryId, fromUin);
    if (q_check_ptr(user))
        message->setName(user->nickname());
    member->addMessage(message);

    QVariantList attributes;
    attributes << UQQCategory::IllegalCategoryId << dstUin;

    TEST(onMessageSended(member->gid(), member->uin(), readFile("test/retok.txt")));
    post(SendBuddyMessageAction, url, QUrl::toPercentEncoding(p, "=&"), attributes);
}

void UQQClient::onMessageSended(quint64 gid, const QString &uin, const QByteArray &data) {
    int retCode = NoError;
    getResponseResult(data, &retCode);

    if (retCode == NoError) {
        qDebug() << "gid:" << gid << "uin:" << uin;
        qDebug() << QTime::currentTime().toString("hh:mm:ss") << "send ok";
    }
}

void UQQClient::sendGroupMessage(quint64 gid, QString content) {
    qDebug() << QTime::currentTime().toString("hh:mm:ss") << "send group message...";

    UQQCategory *group = m_group->getGroupById(gid);
    if (!q_check_ptr(group)) return;

    UQQMessage *message = new UQQMessage(group);
    QString fromUin = getLoginInfo("uin").toString();
    message->setType(TYPE_SEND);
    message->setSrc(fromUin);
    message->setDst(QString::number(gid));
    message->setContent(content);
    message->setTime(QDateTime::currentDateTime());
    group->addMessage(message);

    QUrl url("http://d.web2.qq.com/channel/send_qun_msg2");
    QString p = "r=" + groupMessageData(QString::number(gid), content);
    p.append(QString("&clientid=%1&psessionid=%2").arg(getLoginInfo("clientid").toString(), getLoginInfo("psessionid").toString()));

    QVariantList attributes;
    attributes << gid;

    TEST(onMessageSended(gid, QString::number(gid), readFile("test/retok.txt")));
    post(SendGroupMessageAction, url, QUrl::toPercentEncoding(p, "=&"), attributes);
}

void UQQClient::getGroupSig(quint64 gid, QString dstUin) {
    qDebug() << "request group sig...";

    QUrl url("http://d.web2.qq.com/channel/get_c2cmsg_sig2");
    QUrlQuery query;
    query.addQueryItem("id", QString::number(gid));
    query.addQueryItem("to_uin", dstUin);
    query.addQueryItem("service_type", QString::number(0));
    query.addQueryItem("clientid", getLoginInfo("clientid").toString());
    query.addQueryItem("psessionid", getLoginInfo("psessionid").toString());
    query.addQueryItem("t", getTimestamp());
    url.setQuery(query);
    qDebug() << url.toString();

    QVariantList attributes;
    attributes << gid << dstUin;

    TEST(parseGroupSig(gid, dstUin, readFile("test/group_sig.txt")));
    get(GetGroupSigAction, url, attributes);
}

void UQQClient::parseGroupSig(quint64 gid, const QString &dstUin, const QByteArray &data) {
    UQQMember *member = Q_NULLPTR;
    const QVariantMap &result = getResponseResult(data).toMap();

    if (!result.isEmpty()) {
        member = this->member(gid, dstUin);

        if (q_check_ptr(member))
            member->setGroupSig(result.value("value").toString());
        qDebug() << "request group sig done.";
    }
}

void UQQClient::sendSessionMessage(quint64 gid, QString dstUin, QString content) {
    qDebug() << "send session message...";
    QString fromUin = getLoginInfo("uin").toString();
    UQQCategory *group = m_group->getGroupById(gid);
    if (!q_check_ptr(group)) return;

    UQQMember *member = group->member(dstUin);
    if (!q_check_ptr(member)) return;

    QUrl url("http://d.web2.qq.com/channel/send_sess_msg2");
    QString p = "r=" + sessionMessageData(gid, dstUin, content);
    p.append(QString("&clientid=%1&psessionid=%2").arg(getLoginInfo("clientid").toString(), getLoginInfo("psessionid").toString()));

    if (member->groupSig().isEmpty()) {
        qWarning() << "group sig is empty";
        return;
    }

    UQQMessage *message = new UQQMessage(member);
    message->setType(TYPE_SEND);
    message->setSrc(fromUin);
    message->setDst(dstUin);
    message->setContent(content);
    message->setTime(QDateTime::currentDateTime());
    member->addMessage(message);

    QVariantList attributes;
    attributes << gid << dstUin;

    TEST(onMessageSended(gid, dstUin, readFile("test/retok.txt")));
    post(SendSessionMessageAction, url, QUrl::toPercentEncoding(p, "=&"), attributes);

}

void UQQClient::poll() {
    qDebug() << QTime::currentTime().toString("hh:mm:ss") << "begin poll...";

    QVariantMap param;
    QUrl url("http://d.web2.qq.com/channel/poll2");

    param.insert("clientid", getLoginInfo("clientid"));
    param.insert("psessionid", getLoginInfo("psessionid"));
    param.insert("key", 0);
    param.insert("ids", QVariantList());
    QJsonDocument doc;
    doc.setObject(QJsonObject::fromVariantMap(param));
    QString p = "r=" + doc.toJson() + QString("&clientid=%1&psessionid=%2")
            .arg(getLoginInfo("clientid").toString(), getLoginInfo("psessionid").toString());

    TEST(parsePoll(readFile("test/hello_msg.txt")));
    TEST(parsePoll(readFile("test/groupmsg.txt")));
    TEST(parsePoll(readFile("test/sess_msg.txt")));
    post(PollMessageAction, url, QUrl::toPercentEncoding(p, "=&"));
}

void UQQClient::parsePoll(const QByteArray &data) {
    QString pollType;
    QVariantMap m;
    int retCode = NoError;
    const QVariantList &result = getResponseResult(data, &retCode).toList();

    if (retCode == NoError) {
        for (int i = 0; i < result.size(); i++) {
            m = result.at(i).toMap();
            pollType = m.value("poll_type").toString();
            m = m.value("value").toMap();
            if (pollType == "buddies_status_change") {
                pollStatusChanged(m);
            } else if (pollType == "message") {
                pollMemberMessage(m);
            } else if (pollType == "kick_message") {
                //qDebug() << data;
                pollKickMessage(m);
            } else if (pollType == "group_message") {
                pollGroupMessage(m);
            } else if (pollType == "sess_message") {
                //qDebug() << data;
                pollSessionMessage(m);
            } else if (pollType == "input_notify") {
                //qDebug() << data;
                pollInputNotify(m);
            } else {
                qWarning() << "Unknown poll type:" << pollType;
                qDebug() << data;
            }
        }
    } else if (retCode == PollNormalReturn) {
        //qDebug() << "poll normal return";
    } else if (retCode == PollOfflineError) {
        qDebug() << "Poll error:" << PollOfflineError;
    } else {
        qWarning() << "parsePoll:" << data;
    }
    qDebug() << QTime::currentTime().toString("hh:mm:ss") << "poll done.";
    emit pollReceived();
}

void UQQClient::pollStatusChanged(const QVariantMap &m) {
    UQQMember *member;
    QString uin = m.value("uin").toString();
    int status = UQQMember::statusIndex(m.value("status").toString());
    int clientType = m.value("client_type").toInt();

    member = this->member(UQQCategory::IllegalCategoryId, uin);
    if (!q_check_ptr(member)) return;

    int oldStatus = member->status();
    if (oldStatus != status || member->clientType() != clientType) {
        m_contact->setBuddyStatus(uin, status, clientType);
        if (oldStatus != status) {
            if (oldStatus == UQQMember::OfflineStatus)
                emit buddyOnline(uin);
            emit buddyStatusChanged(member->gid(), uin);
        }
    }
}

void UQQClient::pollInputNotify(const QVariantMap &m) {
    QString fromUin = m.value("from_uin").toString();
    UQQMember *member = this->member(UQQCategory::IllegalCategoryId, fromUin);

    if (q_check_ptr(member))
        member->setInputNotify(true);
}

UQQMessage *UQQClient::parseMessage(QString fromUin, const QVariantMap &m) {
    QVariantList contentList;
    QVariantList fontList;
    QString content;
    UQQMessage *message = new UQQMessage();

    message->setSrc(fromUin);
    message->setDst(m.value("to_uin").toString());
    message->setId(m.value("msg_id").toInt());
    message->setId2(m.value("msg_id2").toInt());
    message->setType(m.value("msg_type").toInt());
    message->setReplyIP(m.value("reply_ip").toUInt());

    QDateTime datetime =
            QDateTime::fromMSecsSinceEpoch(m.value("time").toLongLong() * 1000); // s -> ms
    message->setTime(datetime);

    /*
     * "content":[["font",{"size":10,"color":"000000","style":[0,0,0],"name":"\u5B8B\u4F53"}],
     * "hello",["face",14],"world "]
     */
    contentList = m.value("content").toList();
    if (contentList.first().type() == QVariant::List) {
        fontList = contentList.takeFirst().toList();

    } else {
        qWarning() << "font list not found!";
    }

    foreach(QVariant value, contentList) {
        if (value.type() == QVariant::String) { // common text message
            content.append(value.toString());
        } else if (value.type() == QVariant::List) {    // face number
            const QVariantList &face = value.toList(); // face in the content just like: ':face1'
            content.append("[");
            foreach(QVariant v, face) {
                content.append(v.toString());
            }
            content.append("]");
        } else {
            qWarning() << "unknown message type:" << value.typeName();
        }
    }

    message->setContent(content);

    return message;
}

void UQQClient::pollMemberMessage(const QVariantMap &m) {
    QString src = m.value("from_uin").toString();
    UQQMember *member = this->member(UQQCategory::IllegalCategoryId, src);
    if (!q_check_ptr(member)) return;

    UQQMessage *message = parseMessage(src, m);
    message->setParent(member);
    message->setName(member->markname() == "" ? member->nickname() : member->nickname());
    member->addMessage(message);
    member->setInputNotify(false);

    emit memberMessageReceived(member->gid());
}

void UQQClient::pollGroupMessage(const QVariantMap &m) {
    quint64 gcode = m.value("group_code").toULongLong();
    UQQCategory *group = m_group->getGroupByCode(gcode);
    if (!q_check_ptr(group)) return;

    QString fromUin = m.value("send_uin").toString();
    UQQMessage *message = parseMessage(fromUin, m);
    message->setParent(group);
    group->addMessage(message);

    if (group->messageMask() == UQQCategory::MessageNotify)
        emit groupMessageReceived(group->id());
}

void UQQClient::pollSessionMessage(const QVariantMap &m) {
    QString fromUin = m.value("from_uin").toString();
    quint64 gid = m.value("id").toULongLong();
    UQQMember *member = Q_NULLPTR;

    UQQMessage *message = parseMessage(fromUin, m);

    if ((member = this->member(gid, fromUin)) != Q_NULLPTR) {
        message->setParent(member);
        message->setName(member->card() == "" ? (member->markname() == "" ? member->nickname() : member->nickname()) : member->card());
        member->addMessage(message);
        //emit sessionMessageReceived(group->id());
    } else {
        m_contact->addSessMessage(message);
        getStrangerInfo(gid, fromUin);
        emit memberMessageReceived(UQQCategory::StrangerCategoryId);
    }
}

// {"way":"poll","show_reason":1,"reason":"reason msg"}
void UQQClient::pollKickMessage(const QVariantMap &m) {
    qDebug() << "pollKickMessage";
    //bool showReason = m.value("show_reason").toBool();
    //if (showReason)
    //    addLoginInfo("errMsg", m.value("reason").toString());

    emit kicked(m.value("reason").toString());
}

QList<QObject *> UQQClient::getContactList() {
    QList<QObject *> categories;
    QList<UQQCategory *> &list = m_contact->categories();
    for (int i = 0; i < list.size(); i++) {
        categories.append(list.at(i));
    }
    return categories;
}

QList<QObject *> UQQClient::getGroupList() {
    QList<QObject *> groups;
    QList<UQQCategory *> &list = m_group->groups();
    for (int i = 0; i < list.size(); i++) {
        groups.append(list.at(i));
    }
    return groups;
}

QList<QObject *> UQQClient::getMember(QString uin) {
    QList<QObject *> members;
    UQQMember *member = Q_NULLPTR;
    member = this->member(UQQCategory::IllegalCategoryId, uin);
    if (q_check_ptr(member))
        members.append(member);

    return members;
}

QList<QObject *> UQQClient::getCategoryMembers(quint64 catid) {
    //qDebug() << "get category members...";
    QList<QObject *> list;
    const QList<UQQMember *> &members = m_contact->membersInCategory(catid, true);

    for (int i = 0; i < members.size(); i++) {
        list.append(members.at(i));
    }
    //qDebug() << "category members:" << list.size();
    return list;
}

QList<QObject *> UQQClient::getGroupMembers(quint64 gid) {
    QList<QObject *> list;
    const QList<UQQMember *> &members = m_group->memberInGroup(gid, true);

    for (int i = 0; i < members.size(); i++) {
        list.append(members.at(i));
    }
    return list;
}

QString UQQClient::imageFormat(const QByteArray &data) {
    QByteArray jpg = QByteArray("ffd8");
    QByteArray png = QByteArray("89504e470d0a1a0a");
    QByteArray bmp = QByteArray("424d");
    QByteArray gif = QByteArray("4749463839");
    QByteArray header = data.left(8).toHex();
    //qDebug() << header.toHex();

    if (header.startsWith(png)) {
        return ".png";
    } else if (header.startsWith(jpg)) {
        return ".jpg";
    } else if (header.startsWith(bmp)) {
        return ".bmp";
    } else if (header.startsWith(gif)) {
        return ".gif";
    } else {
        qWarning() << "Unknown image format, prefix:" << header;
        return "";
    }
}

int UQQClient::parseParamList(const QString &data, QStringList &paramList) {
    int begIndex = data.indexOf("(");
    int endIndex = data.lastIndexOf(")");

    QStringList list = data.mid(begIndex + 1, endIndex - begIndex - 1).split(",");
    for (int i = 0; i < list.size(); i++) {
        QString str = list.at(i).trimmed();
        // remove the "'" around the string
        list.replace(i, str.mid(1, str.length() - 2));
    }
    paramList << list;
    return list.size();
}

void UQQClient::addLoginInfo(const QString &key, const QVariant &value) {
    m_loginInfo.insert(key, value);
}
QVariant UQQClient::getLoginInfo(const QString key) const {
    return m_loginInfo.value(key, "");
}

QString UQQClient::getCookie(const QString &name, QUrl url) const {
    if (!m_manager) return "";

    QList<QNetworkCookie> cookies = m_manager->cookieJar()->cookiesForUrl(url);
    for (int i = 0; i < cookies.size(); i++) {
        //qDebug() << "name:" << cookies.at(i).name() << ", value:" << cookies.at(i).value();
        if (cookies.at(i).name() == name) {
            return cookies.at(i).value();
        }
    }
    return "";
}

QVariant UQQClient::getConfig(const QString &key) const {
    return m_config.value(key, "");
}
void UQQClient::addConfig(const QString &key, const QVariant &value) {
    m_config.insert(key, value);
}

QString UQQClient::getClientId() {
    // JS:  = String(k.random(0, 99)) + String((new Date()).getTime() % 1000000)
    qint64 ms = QDateTime::currentDateTimeUtc().toMSecsSinceEpoch();
    qsrand(ms);
    int rand = qrand() % 100;
    QString id = QString::number(rand) + QString::number(ms % 1000000);
    //qDebug() << "clientId:" << id;
    return id;
}

int     UQQClient::getRandomInt(int max) {
    qint64 ms = QDateTime::currentDateTimeUtc().toMSecsSinceEpoch();
    qsrand(ms);
    return qrand() % max;
}

QString UQQClient::getRandom() {
    qint64 ms = QDateTime::currentDateTimeUtc().toMSecsSinceEpoch();
    qsrand(ms);
    qreal r = qrand() / (qreal)ms;
    QString rs = QString::number(r, 'g', 14);
    //qDebug() << "random:" << rs;
    return rs;
}

QString UQQClient::getTimestamp() {
    qint64 ms = QDateTime::currentDateTimeUtc().toMSecsSinceEpoch();
    QString ts = QString::number(ms);
    //qDebug() << "timestamp:" << ts;
    return ts;
}

QByteArray UQQClient::readFile(const QString &filename) {
    QByteArray data;
    if (!filename.isEmpty() && QFile::exists(filename)) {
        QFile file(filename);
        file.open(QIODevice::ReadOnly);
        data = file.readAll();
        file.close();
    }
    return data;
}
