#include <QtCore>
#include <iostream>

static void output(const QJsonObject &object, bool fragment = false) {
    auto bytes = QJsonDocument(object).toJson(QJsonDocument::Compact) + '\n';
    if (fragment) {
        std::cout.write(bytes.data(), bytes.size() / 2);
        std::cout.flush();
        QThread::msleep(8);
        std::cout.write(bytes.data() + bytes.size() / 2, bytes.size() - bytes.size() / 2);
    } else
        std::cout.write(bytes.data(), bytes.size());
    std::cout.flush();
}
static void record(const QJsonObject &object) {
    QFile log(qEnvironmentVariable("GHE_TEST_LOG"));
    if (!log.fileName().isEmpty() && log.open(QIODevice::Append))
        log.write(QJsonDocument(object).toJson(QJsonDocument::Compact) + '\n');
}
int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    auto args = app.arguments();
    args.removeFirst();
    record({{"arguments", QJsonArray::fromStringList(args)},
            {"tokenPresent", qEnvironmentVariableIsSet("GH_TOKEN")},
            {"cwd", QDir::currentPath()}});
    if (args.contains("app-server")) {
        int threads = 0;
        QString thread;
        std::string line;
        while (std::getline(std::cin, line)) {
            auto object = QJsonDocument::fromJson(QByteArray::fromStdString(line)).object();
            record(object);
            if (!object.contains("method"))
                continue;
            const auto method = object["method"].toString(),
                       mode = qEnvironmentVariable("GHE_FAKE_CODEX_MODE");
            auto params = object["params"].toObject();
            auto id = object["id"];
            auto reply = [&](QJsonObject result) { output({{"id", id}, {"result", result}}, true); };
            if (method == "initialize")
                reply({{"userAgent", "fake"}});
            else if (method == "account/read") {
                QJsonObject account = qEnvironmentVariable("GHE_FAKE_CODEX_ACCOUNT") == "apiKey"
                                          ? QJsonObject{{"type", "apiKey"}}
                                          : QJsonObject{{"type", "chatgpt"},
                                                        {"email", "test@example.invalid"},
                                                        {"planType", "plus"}};
                reply({{"account", account}, {"requiresOpenaiAuth", true}});
            } else if (method == "account/login/start") {
                const bool device = params["type"].toString() == "chatgptDeviceCode";
                reply(device ? QJsonObject{{"type", "chatgptDeviceCode"},
                                           {"loginId", "login_test"},
                                           {"verificationUrl", "https://auth.openai.com/codex/device"},
                                           {"userCode", "ABCD-EFGH"}}
                             : QJsonObject{{"type", "chatgpt"},
                                           {"loginId", "login_test"},
                                           {"authUrl", "https://auth.openai.com/authorize?test=1"}});
                if (mode != "pending_auth")
                    output({{"method", "account/login/completed"},
                            {"params", QJsonObject{{"loginId", "login_test"}, {"success", true}}}});
            } else if (method == "account/login/cancel") {
                reply({});
                output({{"method", "account/login/completed"},
                        {"params", QJsonObject{{"loginId", "login_test"}, {"success", true}}}});
            } else if (method == "account/logout")
                reply({});
            else if (method == "thread/start") {
                thread = "thr_" + QString::number(++threads);
                reply({{"thread", QJsonObject{{"id", thread}}}});
            } else if (method == "turn/start") {
                if (mode == "error") {
                    output({{"id", id},
                            {"error", QJsonObject{{"code", -32000}, {"message", "Quota de test épuisé"}}}});
                    continue;
                }
                const auto active = params["threadId"].toString();
                reply({{"turn", QJsonObject{{"id", "turn_test"}, {"status", "inProgress"}}}});
                if (mode == "slow")
                    QThread::msleep(450);
                if (mode == "tool") {
                    output({{"method", "item/commandExecution/requestApproval"},
                            {"id", 999},
                            {"params", QJsonObject{{"threadId", active}}}});
                    output({{"method", "item/started"},
                            {"params", QJsonObject{{"threadId", active},
                                                   {"item", QJsonObject{{"type", "commandExecution"},
                                                                        {"id", "forbidden"}}}}}});
                    continue;
                }
                QString text =
                    mode == "malformed"
                        ? "not json"
                        : QString::fromUtf8(
                              QJsonDocument(QJsonObject{{"summary", "Ajouter le fichier de test"},
                                                        {"description", "Préparer une base de travail."}})
                                  .toJson(QJsonDocument::Compact));
                output({{"method", "item/agentMessage/delta"},
                        {"params", QJsonObject{{"threadId", active}, {"delta", text.left(4)}}}},
                       true);
                output({{"method", "item/completed"},
                        {"params", QJsonObject{{"threadId", active},
                                               {"item", QJsonObject{{"type", "agentMessage"},
                                                                    {"phase", "final_answer"},
                                                                    {"text", text}}}}}},
                       true);
                output({{"method", "turn/completed"},
                        {"params",
                         QJsonObject{{"threadId", active},
                                     {"turn", QJsonObject{{"id", "turn_test"}, {"status", "completed"}}}}}},
                       true);
            } else if (object.contains("id"))
                reply({});
        }
        return 0;
    }
    if (args.value(0) == "auth") {
        if (args.value(1) == "login" || args.value(1) == "refresh") {
            if (qEnvironmentVariableIntValue("GHE_FAKE_GH_AUTH_DENIED")) return 1;
            std::cout << "First copy your one-time code: ABCD-EFGH\nhttps://github.com/login/device\n";
        }
        return 0;
    }
    if (args.value(0) == "api") {
        if(qEnvironmentVariableIntValue("GHE_FAKE_API_DELAY")>0)QThread::msleep(qEnvironmentVariableIntValue("GHE_FAKE_API_DELAY"));
        QString endpoint = args.value(3);
        const bool post = args.contains("POST");
        const auto method=args.contains("--method")?args.value(args.indexOf("--method")+1):QString("GET");QJsonObject payload;
        if ((post||method=="PATCH")&&!endpoint.startsWith("https://uploads.github.com/")) {
            QByteArray input;
            std::string line;
            while (std::getline(std::cin, line))
                input += QByteArray::fromStdString(line);
            payload=QJsonDocument::fromJson(input).object();record({{"payload", payload}});
        }
        if(endpoint.startsWith("user/memberships/orgs")){
            QJsonArray memberships;
            auto membership=[](QString login,QString role="member",QString state="active"){return QJsonObject{{"organization",QJsonObject{{"login",login}}},{"role",role},{"state",state}};};
            if(qEnvironmentVariable("GHE_FAKE_ORG_PAGES")=="1"){
                if(endpoint.contains("page=2"))memberships.append(membership("second-page"));else for(int i=0;i<100;++i)memberships.append(membership("team-org"));
            }else memberships={membership("team-org"),membership("public-only"),membership("blocked-org"),membership("admin-org","admin"),membership("invited-org","member","pending")};
            std::cout<<QJsonDocument(memberships).toJson(QJsonDocument::Compact).constData();return 0;
        }
        if(post&&(endpoint=="user/repos"||(endpoint.startsWith("orgs/")&&endpoint.endsWith("/repos")))){
            if(qEnvironmentVariable("GHE_FAKE_CREATE_ERROR")=="1"){output({{"message","Validation Failed"},{"errors",QJsonArray{QJsonObject{{"field","name"},{"code","already_exists"}}}}});std::cerr<<"HTTP 422 Validation Failed";return 1;}
            auto owner=endpoint=="user/repos"?QString("octocat"):endpoint.section('/',1,1);auto name=payload["name"].toString("created");output({{"name",name},{"full_name",owner+"/"+name},{"clone_url","https://github.com/"+owner+"/"+name+".git"},{"html_url","https://github.com/"+owner+"/"+name},{"private",payload["private"]},{"default_branch",payload["auto_init"].toBool()?"main":""}});return 0;
        }
        if(endpoint.startsWith("orgs/")){
            auto login=endpoint.section('/',1,1);const bool denied=login=="blocked-org"||login=="admin-org";
            output({{"login",login},{"members_can_create_repositories",!denied},{"members_can_create_private_repositories",!denied&&login!="public-only"},{"members_can_create_public_repositories",!denied}});return 0;
        }
        QString route=endpoint;if(endpoint.startsWith("https://uploads.github.com/"))route=QUrl(endpoint).path().mid(1);
        if(route.startsWith("repos/")){
            const auto parts=route.section('?',0,0).split('/');const auto repo=parts.value(1)+"/"+parts.value(2);QJsonObject store;QFile stored(qEnvironmentVariable("GHE_FAKE_SERVICE_STORE"));if(!stored.fileName().isEmpty()&&stored.open(QIODevice::ReadOnly))store=QJsonDocument::fromJson(stored.readAll()).object();stored.close();
            if(parts.size()==3){output({{"full_name",repo},{"default_branch","main"},{"permissions",QJsonObject{{"push",qEnvironmentVariable("GHE_FAKE_READ_ONLY")!="1"}}}});return 0;}
            auto entries=store.value(repo).toArray();if(!store.contains(repo))entries=QJsonArray{
                QJsonObject{{"id",41},{"tag_name","v1.0.0"},{"name","Version stable"},{"body","Notes de "+repo},{"target_commitish","main"},{"draft",false},{"prerelease",false},{"immutable",false},{"html_url","https://github.com/"+repo+"/releases/tag/v1.0.0"},{"assets",QJsonArray{QJsonObject{{"id",71},{"name","AppImage.zip"},{"size",1048576},{"download_count",12},{"browser_download_url","https://github.com/"+repo+"/releases/download/v1.0.0/AppImage.zip"}}}}},
                QJsonObject{{"id",42},{"tag_name","v1.1.0"},{"name","Prochaine version"},{"body","Brouillon"},{"target_commitish","main"},{"draft",true},{"prerelease",true},{"assets",QJsonArray{}}}};
            auto save=[&]{store[repo]=entries;QSaveFile f(qEnvironmentVariable("GHE_FAKE_SERVICE_STORE"));if(!f.fileName().isEmpty()&&f.open(QIODevice::WriteOnly)){f.write(QJsonDocument(store).toJson());f.commit();}};
            if(endpoint.startsWith("https://uploads.github.com/")){
                const auto file=args.value(args.indexOf("--input")+1);const auto filename=QUrlQuery(QUrl(endpoint)).queryItemValue("name",QUrl::FullyDecoded);record({{"uploadFile",file},{"uploadName",filename},{"uploadBytes",double(QFileInfo(file).size())}});
                if(qEnvironmentVariable("GHE_FAKE_UPLOAD_ERROR")=="1"){output({{"message","Upload failed for test"}});return 1;}
                QJsonObject asset{{"id",99},{"name",filename},{"size",double(QFileInfo(file).size())},{"download_count",0},{"browser_download_url","https://github.com/"+repo+"/releases/download/v2.0/"+filename}};for(int i=0;i<entries.size();++i){auto r=entries[i].toObject();if(r["id"].toInt()==parts.value(4).toInt()){auto files=r["assets"].toArray();files.append(asset);r["assets"]=files;entries[i]=r;}}save();output(asset);return 0;
            }
            if(parts.value(3)=="releases"&&parts.size()==4){
                if(post){auto r=payload;r["id"]=100;r["html_url"]="https://github.com/"+repo+"/releases/tag/"+r["tag_name"].toString();r["assets"]=QJsonArray{};entries.prepend(r);save();output(r);return 0;}
                auto bytes=QJsonDocument(entries).toJson(QJsonDocument::Compact);std::cout<<bytes.constData();return 0;
            }
            if(parts.value(4)=="assets"&&method=="DELETE"){for(int i=0;i<entries.size();++i){auto r=entries[i].toObject();QJsonArray files;for(const auto &asset:r["assets"].toArray())if(asset.toObject()["id"].toInt()!=parts.value(5).toInt())files.append(asset);r["assets"]=files;entries[i]=r;}save();return 0;}
            for(int i=0;i<entries.size();++i){auto r=entries[i].toObject();if(r["id"].toInt()!=parts.value(4).toInt())continue;if(method=="DELETE"){entries.removeAt(i);save();return 0;}if(method=="PATCH"){for(auto it=payload.begin();it!=payload.end();++it)r[it.key()]=it.value();entries[i]=r;save();output(r);return 0;}output(r);return 0;}
        }
        if (endpoint == "user") {
            output({{"login", "octocat"}, {"id", 123}, {"name", "Test User"}});
            return 0;
        }
        if (endpoint.startsWith("user/repos")) {
            QJsonArray repos;
            const int count = endpoint.contains("page=2") ? 1 : 100;
            for (int i = 0; i < count; ++i) {
                QString name = QString("project-%1").arg(endpoint.contains("page=2") ? 100 : i);
                repos.append(QJsonObject{{"id", i},
                                         {"name", name},
                                         {"full_name", "octocat/" + name},
                                         {"description", "Description " + name},
                                         {"private", i % 2 == 0},
                                         {"default_branch", "main"},
                                         {"clone_url", "https://github.com/octocat/" + name + ".git"},
                                         {"html_url", "https://github.com/octocat/" + name}});
            }
            auto bytes = QJsonDocument(repos).toJson(QJsonDocument::Compact);
            std::cout << bytes.constData();
            return 0;
        }
    }
    std::cerr << "Unsupported fixture command";
    return 1;
}
