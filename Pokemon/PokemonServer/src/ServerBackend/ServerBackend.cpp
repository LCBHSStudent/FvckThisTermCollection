﻿#include "ServerBackend.h"

#include <MessageTypeGlobal.h>
#include "../StorageHelper/StorageHelper.h"

// ------防止debug下parse bytes类型时包含中文后出现的内存读写bug
// #define AVOID_PROTOBUF_EXCEPTION_FLAG

// ------控制部分debug信息打印
// #undef DEBUG_FLAG

#undef  NET_SLOT
#define NET_SLOT(_name) \
    void ServerBackend::slot##_name(QTcpSocket* client, QByteArray data)

#define CONNECT_EVENT(_eventName)                    \
    connect(                                         \
        m_helper, &NetworkHelper::sig##_eventName,   \
        this,     &ServerBackend::slot##_eventName)  \

#define CALL_SLOT(_name) \
    slot##_name(client, QByteArray(data.data()+4, data.size()-4))

// -----------------Progress Before Sending Network Data------------------ //
#define PROC_PROTODATA(_messageType, _dataBlockName)                        \
    auto byteLength = _dataBlockName.ByteSizeLong();                        \
    auto pData      = new char[byteLength + 2*sizeof(uint32)];              \
    *(reinterpret_cast<uint32*>(pData) + 1) = MessageType::_messageType;    \
    *reinterpret_cast<uint32*>(pData)       = (uint32)byteLength + 4;       \
                                                                            \
    _dataBlockName.SerializeToArray(pData + 2*sizeof(uint32), byteLength);  \
                                                                            \
    m_helper->sendToClient(                                                 \
            client, QByteArray(pData, byteLength + 2*sizeof(uint32)));      \
    delete[] pData                                                          \
// ----------------------------------------------------------------------- //
#define PROC_PROTODATA_WITH_DEST(_messageType, _dataBlockName, _client)     \
    auto byteLength = _dataBlockName.ByteSizeLong();                        \
    auto pData      = new char[byteLength + 2*sizeof(uint32)];              \
    *(reinterpret_cast<uint32*>(pData) + 1) = MessageType::_messageType;    \
    *reinterpret_cast<uint32*>(pData)       = (uint32)byteLength + 4;       \
                                                                            \
    _dataBlockName.SerializeToArray(pData + 2*sizeof(uint32), byteLength);  \
                                                                            \
    m_helper->sendToClient(                                                 \
            _client, QByteArray(pData, byteLength + 2*sizeof(uint32)));     \
    delete[] pData                                                          \
// ----------------------------------------------------------------------- //

/**
 * @brief ServerBackend::ServerBackend
 *        server主要逻辑类构造
 */
ServerBackend::ServerBackend():
    m_helper(new NetworkHelper)
{
    StorageHelper::Instance();
    
    createUserTable("_server");
    // 添加服务器精灵
    // m_serverPkm.append()
    
    CONNECT_EVENT(GetMessage);
    // 更换为direct connection?
    CONNECT_EVENT(UserDisconnected);
}

/**
 * @brief ServerBackend::~ServerBackend
 *        server类析构
 */
ServerBackend::~ServerBackend() {
    // RELEASE ALL BATTLE FIELD
    for (auto battle: m_battleFieldList) {
        delete battle;
    }
    // RELEASE TCP SOCKET SERVER HELPER    
    if(m_helper) {
        delete m_helper;
    }
}

/**
 * @brief ServerBackend::createUserTable
 *        创建用户表，记录其拥有的宝可梦信息
 * @param username  {QString} 用户名
 */
void ServerBackend::createUserTable(const QString& username) {
    const QString userTableStat = 
"CREATE TABLE IF NOT EXISTS `user_" + username + "`(\
    PKM_ID      INT             NOT NULL PRIMARY KEY AUTO_INCREMENT,\
    PKM_TYPEID  INT             NOT NULL,\
    PKM_LEVEL   INT             NOT NULL DEFAULT 1,\
    PKM_EXP     INT             NOT NULL DEFAULT 0,\
    PKM_ATK     INT             NOT NULL DEFAULT 6,\
    PKM_DEF     INT             NOT NULL DEFAULT 6,\
    PKM_HP      INT             NOT NULL DEFAULT 12,\
    PKM_SPD     INT             NOT NULL DEFAULT 6\
);";
    StorageHelper::Instance().transaction(userTableStat, StorageHelper::DEFAULT_FUNC);
}

/**
 * @brief ServerBackend::transferPokemon
 *        宝可梦所有权转移
 * @param fromUser  从谁那里拿
 * @param destUser  放到谁那里
 * @param pkmId     宝可梦id
 */
void ServerBackend::transferPokemon(
    const QString&  fromUser,
    const QString&  destUser,
    int             pkmId
) {
    StorageHelper::Instance().transaction(
        "INSERT INTO user_" + destUser + "(" +
        "PKM_TYPEID,PKM_LEVEL,PKM_EXP,PKM_ATK,PKM_DEF,PKM_HP,PKM_SPD) " +
        "SELECT PKM_TYPEID,PKM_LEVEL,PKM_EXP,PKM_ATK,PKM_DEF,PKM_HP,PKM_SPD " +
        "FROM `user_" + fromUser + "` WHERE PKM_ID=?",
        StorageHelper::DEFAULT_FUNC,
        pkmId
    );
    if (fromUser != "_server") {
        StorageHelper::Instance().transaction(
            "DELETE FROM `user_" + fromUser + "` WHERE PKM_ID=?",
            StorageHelper::DEFAULT_FUNC,
            pkmId
        );
        
        int pokemonCnt = 0;
        StorageHelper::Instance().transaction(
            "SELECT count(*) FROM `user_" + fromUser +"`",
            [&pokemonCnt](QSqlQuery& query) {
                pokemonCnt = query.value(0).toInt();
            }
        );
        if (pokemonCnt < 1) {
            
            QList<int> typeIdList = {};
            StorageHelper::Instance().transaction(
                "SELECT PKM_ID FROM `pokemon_info`", 
                [&typeIdList](QSqlQuery& query) {
                    typeIdList.push_back(query.value(0).toInt());
                }
            );
            
            int randIndex = 
                QRandomGenerator::global()->bounded(typeIdList.size());
            
            StorageHelper::Instance().transaction(
                "INSERT INTO `user_" +  fromUser + "`(\
                 PKM_TYPEID) VALUES(?)",
                 StorageHelper::DEFAULT_FUNC,
                 typeIdList[randIndex]
            );
        }
        
    }
}

void ServerBackend::removeUserPokemon(
    const QString&  userName,
    int             pkmId
) {
    try {
        StorageHelper::Instance().transaction(
            "DELETE FROM `user_" + userName + "` WHERE PKM_ID=?",
            StorageHelper::DEFAULT_FUNC,
            pkmId
        );
    } catch(...) {

    }
    
}

void ServerBackend::procAndSendPkmData(
    const QList<int>&                       pkmIdList,
    const QString&                          userName,
    QTcpSocket*                             destSocket,
    UserProtocol::PokemonDataRequestMode    mode
) {
    UserProtocol::UserPokemonDataResponseInfo resInfo = {};
    UserProtocol::PokemonInfo *pPkmInfo               = nullptr;
                
    // -----------------------PROCESS RES INFO---------------------- //
    resInfo.set_mode(mode);
    resInfo.set_username(userName.toStdString());
                
    QList<QString> aliasList = {};
    QList<QString> sdescList = {};
    // ------ 建立对象，转发数据
    for (int i = 0; i < pkmIdList.length(); i++) {
        aliasList.clear();
        sdescList.clear();
        auto pkm = PokemonFactory::CreatePokemon(userName, pkmIdList[i]);
        // 写入info
        pPkmInfo = resInfo.add_pkmdata();
                    
        pPkmInfo->set_id(pkm->get_id());
        pPkmInfo->set_hp(pkm->get_HP());
        pPkmInfo->set_exp(pkm->get_exp());
        pPkmInfo->set_spd(pkm->get_SPD());
        pPkmInfo->set_def(pkm->get_DEF());
        pPkmInfo->set_atk(pkm->get_ATK());
        pPkmInfo->set_level(pkm->get_level());
        pPkmInfo->set_attr(pkm->get_pkmAttr());
        pPkmInfo->set_type(pkm->get_pkmType());
        pPkmInfo->set_typeid_(pkm->get_typeID());
                    
        // 发送中文技能名 & 技能描述
        for (int i = 0; i < 4; i++) {
            StorageHelper::Instance().transaction(
                "SELECT ALIAS, DESCRIPTION FROM `skill_list` WHERE NAME=?",
                [&aliasList, &sdescList](QSqlQuery& query) {
                    aliasList.push_back(query.value(0).toString());
                    sdescList.push_back(query.value(1).toString());
                },
                pkm->getSkill(i)
            );
        }
#ifdef DEBUG_FLAG
        for (int i = 0; i < 4; i++) {
            qDebug() << pkm->getSkill(i);
            qDebug() << aliasList[i] << sdescList[i];            
        }
#endif
#ifndef AVOID_PROTOBUF_EXCEPTION_FLAG
        pPkmInfo->set_name(pkm->get_name().toStdString());
        pPkmInfo->set_desc(pkm->get_desc().toStdString());
                    
        pPkmInfo->set_skill_1(aliasList[0].toStdString());
        pPkmInfo->set_skill_2(aliasList[1].toStdString());
        pPkmInfo->set_skill_3(aliasList[2].toStdString());
        pPkmInfo->set_skill_4(aliasList[3].toStdString());
                    
        pPkmInfo->set_skill_1_desc(sdescList[0].toStdString());
        pPkmInfo->set_skill_2_desc(sdescList[1].toStdString());
        pPkmInfo->set_skill_3_desc(sdescList[2].toStdString());
        pPkmInfo->set_skill_4_desc(sdescList[3].toStdString());
#endif
#ifdef DEBUG_FLAG
        qDebug() << "BYTE SIZE: " << pPkmInfo->ByteSizeLong();
        // pPkmInfo->PrintDebugString();
#endif                    
        delete pkm;
    }
                
    PROC_PROTODATA_WITH_DEST(
        PokemonDataResponse, resInfo, destSocket);
}

/**
 * @brief ServerBackend::slotGetMessage
 *        根据Global Message Type调用对应的处理槽函数
 * @param client    客户端TCP SOCKET
 * @param data      QByteArray封装的数据
 */
void ServerBackend::slotGetMessage(
    QTcpSocket*     client,
    QByteArray      data
) {
    // 拿到报文中的MessageType枚举
    const int type = *reinterpret_cast<int*>(data.data());
    
    switch (type) {
    case MessageType::UserSignUpRequest:
        CALL_SLOT(UserSignUp);
        break;
    case MessageType::UserLoginRequest:
        CALL_SLOT(UserLogin);
        break;
    case MessageType::UserInfoRequest:
        CALL_SLOT(RequestUserInfo);
        break;
    case MessageType::OnlineUserListRequest:
        CALL_SLOT(RequestOnlineUserList);
        break;
    case MessageType::PokemonDataRequest:
        CALL_SLOT(RequestPkmInfo);
        break;
    case MessageType::BattleInviteRequest:
        CALL_SLOT(BattleInvite);
        break;        
    case MessageType::BattleInviteResponse:
        CALL_SLOT(HandleBattleInviteResponse);
        break;
    case MessageType::BattleOperationInfo:
        CALL_SLOT(HandleBattleOperation);
        break;
    case MessageType::TransferPokemonRequest:
        CALL_SLOT(TransferPokemon);
        break;
    case MessageType::BattleGiveupInfo:
        CALL_SLOT(BattleGiveUp);
        break;
        
    default:
        qDebug() << "[SERVER BACKEND] unknown message type";
        break;
    }
}

// -----------------------NETWORK TRANSACTION-------------------------- //
/*@ 参数类型 QTcpSocket* client, const QByteArray data
 *@ 用于处理接收到对应类型的网络信号后，通过protobuf序列化获取信息
 *@ 最后利用PROC_PROTODATA返回响应信息
 */

// 处理用户登录请求
NET_SLOT(UserLogin) {
    UserProtocol::UserLoginRequestInfo reqInfo = {};
    reqInfo.ParseFromArray(data.data(), data.size());
    
    qDebug() << "[ServerBackend] UserLogin";
    reqInfo.PrintDebugString();
    
    bool    flag = false;
    QString userName = QString::fromStdString(reqInfo.username());
    QString userPsw;
    
    StorageHelper::Instance().transaction(
        "SELECT PASSWORD FROM `user_list` WHERE USERNAME=?",
        [&userPsw, &flag](QSqlQuery& query) {
            flag = true;
            userPsw = query.value(0).toString();
        },
        QString::fromStdString(reqInfo.username())
    );
    
    UserProtocol::UserLoginResponseInfo resInfo = {};
    if(flag) {
        if(userPsw == QString::fromStdString(reqInfo.userpsw())) {
            resInfo.set_status(
                UserProtocol::UserLoginResponseInfo_LoginStatus_SUCCESS);
            for(int i = 0; i < m_userList.size(); i++) {
                if (userName == m_userList.at(i).get_name()) {
                    resInfo.set_status(
                        UserProtocol::UserLoginResponseInfo_LoginStatus_SERVER_REFUSED);
                    break;
                }
            }
            // TODO: 将 User 加入 UserList？ -- finished
            {
                User curUser(userName, client);
                m_userList.push_back(curUser);
            }
            
        } else {
            resInfo.set_status(
                UserProtocol::UserLoginResponseInfo_LoginStatus_USERPSW_ERROR);
        }
    } else {
        resInfo.set_status(
            UserProtocol::UserLoginResponseInfo_LoginStatus_USER_NOT_EXISTS);
    }
    
    PROC_PROTODATA(UserLoginResponse, resInfo);
}

// 处理用户注册请求
NET_SLOT(UserSignUp) {
    UserProtocol::UserSignUpRequestInfo reqInfo = {};
    reqInfo.ParseFromArray(data.data(), data.size());
    
    qDebug() << "[ServerBackend] UserSignUp"; 
#ifdef DEBUG_FLAG
    reqInfo.PrintDebugString();
#endif
    int     count = 0;
    QString userName = QString::fromStdString(reqInfo.username());
    
    // 检测用户名是否已经被占用
    StorageHelper::Instance().transaction(
        "SELECT count(*) FROM `user_list` WHERE USERNAME=?",
        [&count](QSqlQuery& query){
            count = query.value(0).toInt();
        },
        userName
    );
    
    // 构造发给客户端的response信息
    UserProtocol::UserSignUpResponseInfo resInfo = {};
    if(count > 0) {
#ifdef DEBUG_FLAG
        qDebug() << "[SERVER BACKEND-SIGN UP] user already exist";
#endif
        // 若count不为0，说明用户已注册，返回状态码USER_ALREADY_EXISTS
        resInfo.set_status(
            UserProtocol::UserSignUpResponseInfo_SignUpStatus_USER_ALREADY_EXISTS);
    } else {
        StorageHelper::Instance().transaction(
            "INSERT INTO user_list("
                "USERNAME, PASSWORD"
            ") VALUES(?, ?)", 
            StorageHelper::DEFAULT_FUNC,
            userName,
            QString::fromStdString(reqInfo.userpsw())
        );
        // 创建用户精灵表
        createUserTable(userName);
        
        QList<int> typeIdList = {};
        StorageHelper::Instance().transaction(
            "SELECT PKM_ID FROM `pokemon_info`", 
            [&typeIdList](QSqlQuery& query) {
                typeIdList.push_back(query.value(0).toInt());
            }
        );
        
        // 随机插入三个精灵
        for (auto i = 0; i < 3; i++) {
            int randIndex = 
                QRandomGenerator::global()->bounded(typeIdList.size());
            
            StorageHelper::Instance().transaction(
                "INSERT INTO `user_" +  userName + "`(\
                 PKM_TYPEID) VALUES(?)",
                 StorageHelper::DEFAULT_FUNC,
                 typeIdList[randIndex]
            );
        }
        
        resInfo.set_status(
            UserProtocol::UserSignUpResponseInfo_SignUpStatus_SUCCESS);
    }
    
    PROC_PROTODATA(UserSignUpResponse, resInfo);
}

// 处理获取用户信息
NET_SLOT(RequestUserInfo) {
    UserProtocol::UserInfoRequest reqInfo = {};
    reqInfo.ParseFromArray(data.data(), data.size());
    
    QString userName = QString::fromStdString(reqInfo.username());
    
    // 获取用户宝可梦信息
    std::vector<int> pkmIdList;
    int pkmCount = 0,
        highLevelPkmCnt = 0;
    StorageHelper::Instance().transaction(
        "SELECT PKM_ID, PKM_LEVEL FROM `user_" + userName + "`",
        [&pkmIdList, &pkmCount, &highLevelPkmCnt](QSqlQuery& query) {
            pkmIdList.push_back(query.value(0).toInt());
            int level = query.value(1).toInt();
            if(level == 15) {
                highLevelPkmCnt++;
            }
            pkmCount++;
        }
    );
    
    int totalBattleTime = -1,
        totalWinnerTime = -1;
    // 获取用户信息(胜场胜率)
    StorageHelper::Instance().transaction(
        "SELECT TOTAL_BATTLE_TIME, WINNER_TIME FROM `user_list` WHERE USERNAME=?",
        [&totalBattleTime, &totalWinnerTime](QSqlQuery& query) {
            totalBattleTime = query.value(0).toInt();
            totalWinnerTime = query.value(1).toInt();
        },
        userName
    );
    // qDebug() << totalBattleTime << totalWinnerTime;
    
    UserProtocol::UserInfoResponse resInfo = {};
    
    if (totalBattleTime == -1 ||
        totalWinnerTime == -1 ||
        pkmIdList.size() == 0
    ) {
        resInfo.set_resstatus(
            UserProtocol::UserInfoResponse_UserInfoResponseStatus_USER_NOT_EXIST);
    } else {
        if (pkmCount >= 20) {
            resInfo.set_pkmamountbadge(
                UserProtocol::UserInfoResponse_BadgeType_GOLDEN);
        } else if(pkmCount >= 10) {
            resInfo.set_pkmamountbadge(
                UserProtocol::UserInfoResponse_BadgeType_SILVER);
        } else {
            resInfo.set_pkmamountbadge(
                UserProtocol::UserInfoResponse_BadgeType_BRONZE);
        }
        
        if (highLevelPkmCnt >= 10) {
            resInfo.set_highlevelbadge(
                UserProtocol::UserInfoResponse_BadgeType_GOLDEN);
        } else if (highLevelPkmCnt >= 5) {
            resInfo.set_highlevelbadge(
                UserProtocol::UserInfoResponse_BadgeType_SILVER);
        } else {
            resInfo.set_highlevelbadge(
                UserProtocol::UserInfoResponse_BadgeType_BRONZE);
        }
        
        for(auto pkmId: pkmIdList) {
            resInfo.add_pokemonid(pkmId);
        }
        
        resInfo.set_timeofduel(totalBattleTime);
        resInfo.set_timeofwins(totalWinnerTime);
        if (totalBattleTime == 0) {
            resInfo.set_winrate(0.0f);
        } else {
            resInfo.set_winrate(
                std::floor(10000.f*
                    static_cast<double>(totalWinnerTime) / 
                    static_cast<double>(totalBattleTime) + 0.5
                ) / 10000.f
            );
        }
        
        
        resInfo.set_username(reqInfo.username());
        resInfo.set_resstatus(
            UserProtocol::UserInfoResponse_UserInfoResponseStatus_SUCCESS);
    }
#ifdef DEBUG_FLAG
    resInfo.PrintDebugString();
#endif
    PROC_PROTODATA(UserInfoResponse, resInfo);
}

// 处理用户登出 & 断线
NET_SLOT(UserDisconnected) {
    (void)data;
    
    for (int i = 0; i < m_userList.size(); i++) {
        if (m_userList[i].get_userSocket() == client) {
            // 查找状态，若为对战中则向对手发送胜利报文
            if (m_userList[i].get_status() == User::UserStatus::BATTLING) {
                for (int i = 0; i < m_battleFieldList.size(); i++) {
                    auto battle = m_battleFieldList.at(i);
                    
                    User* dest  = nullptr;
                    bool flag   = false;
                    if (battle->getUserA()->get_name() == m_userList[i].get_name()) {
                        dest = battle->getUserB();
                        flag = true;
                    } else if (battle->getUserB()->get_name() == m_userList[i].get_name()) {
                        dest = battle->getUserA();
                        flag = true;
                    }
                    
                    if (flag) {
                        
                        if (dest != nullptr) {
                            dest->set_status(User::UserStatus::IDLE);
                            
                            BattleProtocol::BattleFinishInfo info = {};
                            info.set_mode(
                                BattleProtocol::BattleFinishInfo_FinishMode_OPPOSITE_DISCONNECTED
                            );
                            info.set_result(
                                BattleProtocol::BattleFinishInfo_BattleResult_WIN
                            );
                            
                            PROC_PROTODATA_WITH_DEST(
                                BattleFinishInfo, info, dest->get_userSocket()
                            );
                        }
                        
                        disconnect(
                            battle, &BattleField::sigTurnInfoReady,
                            this,   &ServerBackend::slotGetTurnInfo
                        );
                        disconnect(
                            battle, &BattleField::sigBattleFinished,
                            this,   &ServerBackend::slotGetBattleResult
                        );
                        delete battle;
                        m_battleFieldList.removeAt(i);
                        
                        break;
                    }
                }
            }
            m_userList.removeAt(i);
        }
    }
    
    qDebug () 
            << "[SERVER BACKEND-DESTROY BATTLE FILED] CURRENT BATTLE FIELD COUNT: " 
            << m_battleFieldList.size();
    qDebug () 
            << "[SERVER BACKEND-REMOVE USER] CURRENT ONLINE USER COUNT: " 
            << m_userList.size();
}

// 处理获取宝可梦信息请求
NET_SLOT(RequestPkmInfo) {
    UserProtocol::UserPokemonDataRequestInfo reqInfo = {};
    reqInfo.ParseFromArray(data.data(), data.size());
    
    QString userName = QString::fromStdString(reqInfo.username());
#ifdef DEBUG_FLAG
    for (auto& user: m_userList) {
        if (user.get_userSocket() == client) {
            qDebug() << "USER SOCKET FOUND";
        }
    }
    qDebug() << "[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[["+userName+"]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]";
    qDebug() << "DATA SIZE" << data.size();
#endif
    
    UserProtocol::UserPokemonDataResponseInfo resInfo   = {};
    UserProtocol::PokemonInfo *pPkmInfo                 = nullptr;
    resInfo.set_mode(reqInfo.mode());
    resInfo.set_username(reqInfo.username());
    
    // ------ 拿到ID
    QList<int> pkmIdList = {};
//    QVector<PokemonBase*> ptrList = {};

    if (reqInfo.reqtype() ==
        UserProtocol::UserPokemonDataRequestInfo_PokemonDataRequestType_ALL
    ) {
        StorageHelper::Instance().transaction(
            "SELECT PKM_ID FROM `user_" + userName + "`",
            [&pkmIdList](QSqlQuery& query) {
                pkmIdList.push_back(query.value(0).toInt());
            }
        );
//        ptrList.resize(pkmIdList.size());
        
    } else if (reqInfo.reqtype() == 
        UserProtocol::UserPokemonDataRequestInfo_PokemonDataRequestType_SPECIFIC
    ) {
        for (int i = 0; i < reqInfo.pokemonid_size(); i++) {
            pkmIdList.push_back(reqInfo.pokemonid(i));
        }
    }
    
    QList<QString> aliasList = {};
    QList<QString> sdescList = {};
    // ------ 建立对象，转发数据
    for (int i = 0; i < pkmIdList.size(); i++) {
        aliasList.clear();
        sdescList.clear();
        auto pkm = PokemonFactory::CreatePokemon(userName, pkmIdList[i]);
        // 写入info
        pPkmInfo = resInfo.add_pkmdata();
        
        pPkmInfo->set_id(pkm->get_id());
        pPkmInfo->set_hp(pkm->get_HP());
        pPkmInfo->set_exp(pkm->get_exp());
        pPkmInfo->set_spd(pkm->get_SPD());
        pPkmInfo->set_def(pkm->get_DEF());
        pPkmInfo->set_atk(pkm->get_ATK());
        pPkmInfo->set_level(pkm->get_level());
        pPkmInfo->set_attr(pkm->get_pkmAttr());
        pPkmInfo->set_type(pkm->get_pkmType());
        pPkmInfo->set_typeid_(pkm->get_typeID());
        
        // 发送中文技能名 & 技能描述
        for (int i = 0; i < 4; i++) {
            StorageHelper::Instance().transaction(
                "SELECT ALIAS, DESCRIPTION FROM `skill_list` WHERE NAME=?",
                [&aliasList, &sdescList](QSqlQuery& query) {
                    aliasList.push_back(query.value(0).toString());
                    sdescList.push_back(query.value(1).toString());
                },
                pkm->getSkill(i)
            );
        }
#ifdef DEBUG_FLAG
        for (int i = 0; i < 4; i++) {
            qDebug() << pkm->getSkill(i);
            qDebug() << aliasList[i] << sdescList[i];            
        }
#endif
#ifndef AVOID_PROTOBUF_EXCEPTION_FLAG
        pPkmInfo->set_name(pkm->get_name().toStdString());
        pPkmInfo->set_desc(pkm->get_desc().toStdString());
        
        pPkmInfo->set_skill_1(aliasList[0].toStdString());
        pPkmInfo->set_skill_2(aliasList[1].toStdString());
        pPkmInfo->set_skill_3(aliasList[2].toStdString());
        pPkmInfo->set_skill_4(aliasList[3].toStdString());
        
        pPkmInfo->set_skill_1_desc(sdescList[0].toStdString());
        pPkmInfo->set_skill_2_desc(sdescList[1].toStdString());
        pPkmInfo->set_skill_3_desc(sdescList[2].toStdString());
        pPkmInfo->set_skill_4_desc(sdescList[3].toStdString());
#endif
        qDebug() << "BYTE SIZE: " << pPkmInfo->ByteSizeLong();
        // pPkmInfo->PrintDebugString();
        
        delete pkm;
    }
    
    // PRINT DEBUG INFO
    {
        qDebug() << "BYTE SIZE: " << resInfo.ByteSizeLong();
        // resInfo.PrintDebugString();        
    }
    
    PROC_PROTODATA(PokemonDataResponse, resInfo);
}

// 处理获取在线用户列表请求
NET_SLOT(RequestOnlineUserList) {
    UserProtocol::OnlineUserListRequestInfo reqInfo = {};
    reqInfo.ParseFromArray(data.data(), data.size());
    
    reqInfo.PrintDebugString();
    
    QString requestUser = QString::fromStdString(reqInfo.username());
    
    UserProtocol::OnlineUserListResponseInfo resInfo = {};
    for (auto& user: m_userList) {
        qDebug() << "[ONLINE USER] " << user.get_name();
        if (user.get_name() != requestUser) {
            UserProtocol::UserStatusInfo info = {};
            info.set_username(user.get_name().toStdString());
            info.set_userstatus((int32_t)user.get_status());
            
            *resInfo.add_userlist() = info;
        }
    }
    
    PROC_PROTODATA(OnlineUserListResponse, resInfo);
}

// 处理收到客户端发送的对战请求信息
// 记得Debug时走一下客户端的登录流程
NET_SLOT(BattleInvite) {
    BattleProtocol::BattleStartRequest info = {};
    info.ParseFromArray(data.data(), data.size());
#ifdef DEBUG_FLAG
    qDebug() << "[SERVER BACKEDN] GET NEW BATTLE INVITE REQUEST";
    info.PrintDebugString();
#endif
    QString fromUser = QString::fromStdString(info.fromuser());
    QString destUser = QString::fromStdString(info.destuser());
    
    User* userA = nullptr;
    User* userB = nullptr;
    
    for (auto& user: m_userList) {
        if (user.get_name() == fromUser) {
            userA = &user;
            break;
        }
    }
    if (userA == nullptr) {
        return;
    }
    if (userA->get_status() == User::UserStatus::BATTLING) {
        return;
    }
    // 设置出战宝可梦
    userA->set_battlePkmId(info.fromuserpkmid());
    
    if (destUser == "_server") {
        {
            auto pkmA = PokemonFactory::CreatePokemon(fromUser, userA->get_battlePkmId());
            auto pkmB = PokemonFactory::CreatePokemon(destUser, info.serverpkmid());
            BattleField* battle = nullptr;
            if (info.battlemode() == BattleProtocol::BattleMode::EXP_BATTLE) {
                battle = new BattleField(
                    userA, nullptr, pkmA, pkmB, BattleField::EXP_BATTLE
                );
            } else {
                battle = new BattleField(
                    userA, nullptr, pkmA, pkmB, BattleField::DUEL_BATTLE
                );
            }
            
            connect(
                battle, &BattleField::sigTurnInfoReady,
                this,   &ServerBackend::slotGetTurnInfo
            );
            connect(
                battle, &BattleField::sigBattleFinished,
                this,   &ServerBackend::slotGetBattleResult
            );
            m_battleFieldList.push_back(battle);
        }
        
        userA->set_status(User::UserStatus::BATTLING);
        
        BattleProtocol::BattleStartResponse resInfo = {};
        resInfo.set_status(BattleProtocol::BattleStartStatus::SUCCESS);
        resInfo.set_isusera(1);
        resInfo.set_tapkmid(info.serverpkmid());
        resInfo.set_urpkmid(userA->get_battlePkmId());
        resInfo.PrintDebugString();
        
        PROC_PROTODATA(BattleStartResponse, resInfo);
        return;
    }
    else {
        for (auto& user: m_userList) {
            if (user.get_name() == destUser) {
                userB = &user;
#ifdef DEBUG_FLAG
                qDebug() << "[BATTLE INVITE] DEST USER FOUND";
#endif
                break;
            }
        }
        
        BattleProtocol::BattleStartResponse resInfo = {};
        if (userB == nullptr) {
            resInfo.set_status(BattleProtocol::BattleStartStatus::DEST_NOT_ONLINE);
        } else {
            if (userB->get_status() == User::UserStatus::BATTLING) {
                resInfo.set_status(BattleProtocol::BattleStartStatus::ALREADY_START);
            } else {
                BattleProtocol::BattleInviteRequest inviteInfo = {};
                inviteInfo.set_fromuser(info.fromuser());
                inviteInfo.set_battlemode(info.battlemode());
                
                inviteInfo.PrintDebugString();
                // 如果找到对方用户，转发对战请求，结束函数
                PROC_PROTODATA_WITH_DEST(
                    BattleInviteRequest, inviteInfo, userB->get_userSocket());
                return;
            }
        }
        resInfo.PrintDebugString();
        // 若未找到或对方正在对战中，则返回response给请求用户
        PROC_PROTODATA(BattleStartResponse, resInfo);
    }
}

// 处理对战邀请应答
NET_SLOT(HandleBattleInviteResponse) {
    (void)client;
    BattleProtocol::BattleInviteResponse resInfo = {};
    resInfo.ParseFromArray(data.data(), data.size());
    
#ifdef DEBUG_FLAG
    qDebug () << "[SERVER BACKEND] GET BATTLE INVITE RESPONSE";
    resInfo.PrintDebugString();
#endif
    
    BattleProtocol::BattleStartResponse startInfoA = {};
    BattleProtocol::BattleStartResponse startInfoB = {};
    startInfoA.set_status(resInfo.flag());
    startInfoB.set_status(resInfo.flag());
    
    QString userNameA   = QString::fromStdString(resInfo.fromuser());
    QString userNameB   = QString::fromStdString(resInfo.destuser());
    User*   pUserA  = nullptr;
    User*   pUserB  = nullptr;
    
    for (auto& user: m_userList) {
        if (user.get_name() == userNameA) {
            pUserA  = &user;
        } else if (user.get_name() == userNameB) {
            pUserB  = &user;
        }
    }
    
    if (pUserA == nullptr || pUserB == nullptr) {
        qDebug() << "[SERVER BACKEND] NULL USER PTR IN HANDLING BATTLE INVITE RESPONSE";
        return;
    }
    
    if (resInfo.flag() != BattleProtocol::BattleStartStatus::SUCCESS) {
        {
            PROC_PROTODATA_WITH_DEST(
                BattleStartResponse, startInfoA, pUserA->get_userSocket());
        }
//        {
//            PROC_PROTODATA_WITH_DEST(
//                BattleStartResponse, startInfoB, pUserB->get_userSocket());
//        }
        return;
    }
    
    pUserB->set_battlePkmId(resInfo.destuserpkmid());
    int pkmId_A = pUserA->get_battlePkmId();
    int pkmId_B = pUserB->get_battlePkmId();
    
    auto pPkmA  = PokemonFactory::CreatePokemon(userNameA, pkmId_A);
    auto pPkmB  = PokemonFactory::CreatePokemon(userNameB, pkmId_B);
    
    BattleField* battle = nullptr;
    if (resInfo.battlemode() == BattleProtocol::BattleMode::EXP_BATTLE) {
        battle = 
            new BattleField(pUserA, pUserB, pPkmA, pPkmB, BattleField::EXP_BATTLE);
    } else {
        battle = 
            new BattleField(pUserA, pUserB, pPkmA, pPkmB, BattleField::DUEL_BATTLE);
    }
    connect(
        battle, &BattleField::sigTurnInfoReady,
        this,   &ServerBackend::slotGetTurnInfo
    );
    connect(
        battle, &BattleField::sigBattleFinished,
        this,   &ServerBackend::slotGetBattleResult
    );
    m_battleFieldList.push_back(battle);
    
    startInfoA.set_isusera(1);
    startInfoB.set_isusera(0);
    
    startInfoA.set_urpkmid(pkmId_A);
    startInfoB.set_tapkmid(pkmId_A);
    startInfoA.set_tapkmid(pkmId_B);
    startInfoB.set_urpkmid(pkmId_B);
    
    startInfoA.PrintDebugString();
    startInfoB.PrintDebugString();
    
    // avoid redefination of variables
    {
        PROC_PROTODATA_WITH_DEST(
            BattleStartResponse, startInfoA, pUserA->get_userSocket());
    }
    {
        PROC_PROTODATA_WITH_DEST(
            BattleStartResponse, startInfoB, pUserB->get_userSocket());
    }
    
    pUserA->set_status(User::UserStatus::BATTLING);
    pUserB->set_status(User::UserStatus::BATTLING);
}

// 处理对战操作
NET_SLOT(HandleBattleOperation) {
    (void)client;
    BattleProtocol::BattleOperationInfo info = {};
    info.ParseFromArray(data.data(), data.size());
    
    info.PrintDebugString();
    
    int     isUserA     = info.isusera();
    int     skillIndex  = info.skillindex();
    QString userName    = QString::fromStdString(info.username());
    
    for (int i = 0; i < m_battleFieldList.size(); i++) {
        auto battle = m_battleFieldList[i];
        if (isUserA) {
            if (battle->getUserA()->get_name() == userName) {
                battle->setAction(skillIndex, 0);
                if (battle->getUserB() == nullptr) {
                    battle->setAction(
                        QRandomGenerator::global()->bounded(4), 1);
                }
            }
        } else {
            if (battle->getUserB()->get_name() == userName) {
                battle->setAction(skillIndex, 1);
            }
        }
    }
}

// 处理宝可梦所有权转移
NET_SLOT(TransferPokemon) {
    UserProtocol::TransferPokemonRequest reqInfo = {};
    reqInfo.ParseFromArray(data.data(), data.size());
    
    
    QString fromUser = QString::fromStdString(reqInfo.fromuser());
    QString destUser = QString::fromStdString(reqInfo.destuser());
    transferPokemon(fromUser, destUser, reqInfo.pkmid());
    
    UserProtocol::TransferPokemonResponse resInfo = {};
    resInfo.set_status(
        UserProtocol::TransferPokemonResponse_TransferPokemonStatus_SUCCESS
    );
    
    PROC_PROTODATA(TransferPokemonResponse, resInfo);
}

/**
 * @brief ServerBackend::slotGetTurnInfo
 *        得到某个BattleField中的对战回合信息
 * @param info {BattleField::TurnInfo} 回合内Hp变化 buff信息
 */
void ServerBackend::slotGetTurnInfo(BattleField::TurnInfo info) {
    BattleProtocol::BattleTurnInfo infoA = {};
    BattleProtocol::BattleTurnInfo infoB = {};
    
    if (info.type == BattleField::A_TO_B) {
        infoA.set_type(BattleProtocol::BattleTurnInfoType::A_TO_B);
#ifndef AVOID_PROTOBUF_EXCEPTION_FLAG
        infoA.set_skillname(info.skillName.toStdString());
#endif
        infoA.set_selfbuffid(info.selfBuff.buffId);
        infoA.set_selfbufflast(info.selfBuff.turnCnt);
        infoA.set_destbuffid(info.destBuff.buffId);
        infoA.set_destbufflast(info.destBuff.turnCnt);
        infoA.set_selfdeltahp(info.selfDeltaHP);
        infoA.set_destdeltahp(info.destDeltaHP);
        
        infoB.set_type(BattleProtocol::BattleTurnInfoType::A_TO_B);
#ifndef AVOID_PROTOBUF_EXCEPTION_FLAG
        infoB.set_skillname(info.skillName.toStdString());
#endif
        infoB.set_selfbuffid(info.destBuff.buffId);
        infoB.set_selfbufflast(info.destBuff.turnCnt);
        infoB.set_destbuffid(info.selfBuff.buffId);
        infoB.set_destbufflast(info.selfBuff.turnCnt);
        infoB.set_selfdeltahp(info.destDeltaHP);
        infoB.set_destdeltahp(info.selfDeltaHP);
    } else {
        infoB.set_type(BattleProtocol::BattleTurnInfoType::B_TO_A);
#ifndef AVOID_PROTOBUF_EXCEPTION_FLAG
        infoB.set_skillname(info.skillName.toStdString());
#endif
        infoB.set_selfbuffid(info.selfBuff.buffId);
        infoB.set_selfbufflast(info.selfBuff.turnCnt);
        infoB.set_destbuffid(info.destBuff.buffId);
        infoB.set_destbufflast(info.destBuff.turnCnt);
        infoB.set_selfdeltahp(info.selfDeltaHP);
        infoB.set_destdeltahp(info.destDeltaHP);
        
        infoA.set_type(BattleProtocol::BattleTurnInfoType::B_TO_A);
#ifndef AVOID_PROTOBUF_EXCEPTION_FLAG
        infoA.set_skillname(info.skillName.toStdString());
#endif
        infoA.set_selfbuffid(info.destBuff.buffId);
        infoA.set_selfbufflast(info.destBuff.turnCnt);
        infoA.set_destbuffid(info.selfBuff.buffId);
        infoA.set_destbufflast(info.selfBuff.turnCnt);
        infoA.set_selfdeltahp(info.destDeltaHP);
        infoA.set_destdeltahp(info.selfDeltaHP);
    }
    infoA.PrintDebugString();
    infoB.PrintDebugString();
    
    auto pBattleField = reinterpret_cast<BattleField*>(sender());
    {
        auto pUserA = pBattleField->getUserA();
        if (pUserA != nullptr) {
            PROC_PROTODATA_WITH_DEST(
                BattleTurnInfo, infoA, pBattleField->getUserA()->get_userSocket());
        }
    }
    {
        auto pUserB = pBattleField->getUserB();
        if (pUserB != nullptr) {
            PROC_PROTODATA_WITH_DEST(
                BattleTurnInfo, infoB, pBattleField->getUserB()->get_userSocket());
        }
    }
}

/**
 * @brief ServerBackend::slotGetBattleResult
 *        处理对战结果，析构并移除对应BattleField & 发送战利品信息 & 获得战利品 & 更新用户信息
 * @param winner {User*} 对战胜者
 */
void ServerBackend::slotGetBattleResult(User* winner) {
    BattleProtocol::BattleFinishInfo infoWinner = {};
    BattleProtocol::BattleFinishInfo infoLoser  = {};
    
    // 拿到发送方BattleField指针
    auto pBattleField = reinterpret_cast<BattleField*>(sender());
    // 获取用户指针
    auto pUserA = pBattleField->getUserA();
    auto pUserB = pBattleField->getUserB();
    
    // 生成胜利者与失败者
    User* loser = nullptr;
    if (winner == pUserA) {
        loser = pUserB;
    } else {
        loser = pUserA;
    }
    
    infoWinner.set_mode(BattleProtocol::BattleFinishInfo_FinishMode_NORMAL);
    infoWinner.set_result(BattleProtocol::BattleFinishInfo_BattleResult_WIN);
    
    infoLoser.set_mode(BattleProtocol::BattleFinishInfo_FinishMode_NORMAL);
    infoLoser.set_result(BattleProtocol::BattleFinishInfo_BattleResult_LOSE);
    
    // 如果是与server对战，则pUserB必定为nullptr
    // 更改用户状态
    if (loser != nullptr) {
        loser->set_status(User::UserStatus::IDLE);
        PROC_PROTODATA_WITH_DEST(
            BattleFinishInfo, infoLoser, loser->get_userSocket());
    }
    if (winner != nullptr) {
        winner->set_status(User::UserStatus::IDLE);
        // 不论对战模式，胜利方都将获取经验
        if (winner == pUserA) {
            pBattleField->getPkmA()->gainExperience(
                pBattleField->getPkmB()->get_level() * 1.5
            );
        } else {
            pBattleField->getPkmB()->gainExperience(
                pBattleField->getPkmA()->get_level() * 1.5
            );
        }
        PROC_PROTODATA_WITH_DEST(
            BattleFinishInfo, infoWinner, winner->get_userSocket());
    }
    
    // std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    // 如果为决斗战
    if (pBattleField->getMode() == BattleField::BattleMode::DUEL_BATTLE) {
        if (winner != nullptr) {
            winner->battleWon();
            
            if (loser != nullptr) {
                loser->battleLose();
                
                QString loserName = loser->get_name();
                auto    pkmIdList = loser->get_pokemonList();
                QList<int> selectIdList;
                
                // 生成要送出的精灵列表
                for (int i = 0; i < 3 && pkmIdList.length() > 0; i++) {
                    int randIndex = 
                        QRandomGenerator::global()->bounded(pkmIdList.length());
                    selectIdList.push_back(pkmIdList[randIndex]);
                    pkmIdList.removeAt(randIndex);
                }
                
                // 处理并发送战利品宝可梦信息
                procAndSendPkmData(
                    selectIdList,
                    loserName,
                    loser->get_userSocket(),
                    UserProtocol::PokemonDataRequestMode::TROPHIE
                );
                
// --------------------------------------------------------------------------------- //                
                
            } else {
                // 直接拿到对战精灵
                transferPokemon(
                    "_server",
                    winner->get_name(),
                    pBattleField->getPkmB()->get_id()
                );
            }
        } else {
            if (loser != nullptr) {
                loser->battleLose();

                QString loserName = loser->get_name();
                auto    pkmIdList = loser->get_pokemonList();
                QList<int> selectIdList;
                
                for (int i = 0; i < 3 && pkmIdList.length() > 0; i++) {
                    int randIndex = 
                        QRandomGenerator::global()->bounded(pkmIdList.length());
                    selectIdList.push_back(pkmIdList[randIndex]);
                    pkmIdList.removeAt(randIndex);
                }
                
                // 处理并发送战利品宝可梦信息
                procAndSendPkmData(
                    selectIdList,
                    loserName,
                    loser->get_userSocket(),
                    UserProtocol::PokemonDataRequestMode::TROPHIE
                );


            }
#ifdef DEBUG_FLAG
            else {
                qDebug() << "?";
            }
#endif
        }
    }
    
    // 断链信号并做清理
    disconnect(
        pBattleField,   &BattleField::sigTurnInfoReady,
        this,           &ServerBackend::slotGetTurnInfo
    );
    disconnect(
        pBattleField,   &BattleField::sigBattleFinished,
        this,           &ServerBackend::slotGetBattleResult
    );
    m_battleFieldList.removeOne(pBattleField);
    delete pBattleField;
}

NET_SLOT(BattleGiveUp) {
    (void)client;
    BattleProtocol::BattleGiveupInfo giveUpInfo = {};
    giveUpInfo.ParseFromArray(data.data(), data.size());
    
    QString giveUpUser = QString::fromStdString(giveUpInfo.username());
    // 遍历对战列表
    for (int i = 0; i < m_battleFieldList.size(); i++) {
        auto battle = m_battleFieldList.at(i);
        
        User* dest = nullptr;
        bool flag  = false;
        if (battle->getUserA()->get_name() == giveUpUser) {
            battle->getUserA()->set_status(User::UserStatus::IDLE);
            dest = battle->getUserB();
            flag = true;
        } else if (battle->getUserB()->get_name() == giveUpUser) {
            battle->getUserB()->set_status(User::UserStatus::IDLE);
            dest = battle->getUserA();
            flag = true;
        }
        
        if (flag) {
            if (dest != nullptr) {
                dest->set_status(User::UserStatus::IDLE);
                
                BattleProtocol::BattleFinishInfo info = {};
                info.set_mode(
                    BattleProtocol::BattleFinishInfo_FinishMode_OPPOSITE_DISCONNECTED
                );
                info.set_result(
                    BattleProtocol::BattleFinishInfo_BattleResult_WIN
                );
                
                
                PROC_PROTODATA_WITH_DEST(
                    BattleFinishInfo, info, dest->get_userSocket()
                );
            }
            
            
            
            disconnect(
                battle, &BattleField::sigTurnInfoReady,
                this,   &ServerBackend::slotGetTurnInfo
            );
            disconnect(
                battle, &BattleField::sigBattleFinished,
                this,   &ServerBackend::slotGetBattleResult
            );
            delete battle;
            m_battleFieldList.removeAt(i);
            
            break;
        }
    }     
}
