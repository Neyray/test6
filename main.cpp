#include "E:\DownLoad\cpp-httplib-master\cpp-httplib-master\httplib.h"
#include "E:\mysql\MySQL Server 8.0\include\mysql.h"
#include<iostream>
#include<fstream>
#include<sstream>
#include "nlohmann/json.hpp"
#include<ctime>
#include<chrono>
#include<iomanip>
// ...existing code...  // 需要安装 nlohmann/json 库
using json = nlohmann::json;
#pragma comment(lib,"ws2_32.lib")

using namespace std;

const std::string DB_HOST = "localhost";
const std::string DB_USER = "root";
const std::string DB_PASS = "l2006821";  // 替换为你的 MySQL 密码
const std::string DB_NAME = "login_demo";
const int DB_PORT = 3306;

MYSQL* connectDB() {
    MYSQL* conn = mysql_init(nullptr);
    if (!conn) {
        std::cerr << "MySQL 初始化失败" << std::endl;
        return nullptr;
    }
    // 连接数据库（明文密码直接传递）
    if (!mysql_real_connect(conn, DB_HOST.c_str(), DB_USER.c_str(), DB_PASS.c_str(),
                           DB_NAME.c_str(), DB_PORT, nullptr, 0)) {
        std::cerr << "数据库连接失败: " << mysql_error(conn) << std::endl;
        mysql_close(conn);
        return nullptr;
    }

    if (mysql_ping(conn) != 0) {
    std::cerr << "Connection lost: " << mysql_error(conn) << std::endl;
    }


    return conn;
}

json executeSelectQuery(MYSQL* conn, const std::string& sql) {
    json result = json::array();

    if (mysql_ping(conn) != 0) {
    std::cerr << "Connection lost: " << mysql_error(conn) << std::endl;
    }

    // 执行 SQL 语句
    if (mysql_query(conn, sql.c_str())) {
        std::cerr << "Query failed: " << mysql_error(conn) << std::endl;
        return result;
    }

    // 获取结果集
    MYSQL_RES* res = mysql_store_result(conn);
    if (!res) {
        std::cerr << "Result fetching failed: " << mysql_error(conn) << std::endl;
        return result;
    }

    // 解析结果集
    MYSQL_FIELD* fields = mysql_fetch_fields(res);
    MYSQL_ROW row;
    while ((row = mysql_fetch_row(res))) {
        json obj;
        for (unsigned int i = 0; i < mysql_num_fields(res); ++i) {
            obj[fields[i].name] = row[i] ? row[i] : "";
        }
        result.push_back(obj);
    }

    // 释放资源
    mysql_free_result(res);
    return result;
}

// 验证用户是否存在（明文密码比对）
bool validateUser(const std::string& identifier, const std::string& password, MYSQL* conn) {
    // 预处理 SQL 语句（防止 SQL 注入）
    MYSQL_STMT* stmt = mysql_stmt_init(conn);
    if (!stmt) return false;
    string sql;
    for(int i=0;i<identifier.length();i++)
    {
        if(identifier[i]=='@')
        {
           sql="SELECT password FROM users WHERE email = '"+identifier+"'";
           break;
        }
        else
        {
           sql="SELECT password FROM users WHERE phone = '"+identifier+"'";
        }
    }
    std::cout<<sql<<endl;
    if (mysql_stmt_prepare(stmt, sql.c_str(), sql.length())) {
        cerr << "预处理失败: " << mysql_stmt_error(stmt) << endl;
        return false;
    }

    if (mysql_ping(conn) != 0) 
    {
        std::cerr << "Connection lost: " << mysql_error(conn) << std::endl;
    }

    // 获取结果
    json result = executeSelectQuery(conn, sql);

    if (mysql_query(conn, sql.c_str())) {
            std::cerr << u8"执行失败: " << mysql_error(conn) << std::endl;
            mysql_close(conn);
            return false;
    }

    MYSQL_RES* res = mysql_store_result(conn);
    if (!res) {
        std::cerr << u8"获取结果集失败: " << mysql_error(conn) << std::endl;
        mysql_close(conn);
        return false;
    }

    mysql_stmt_close(stmt);
    

    MYSQL_ROW row;
    row = mysql_fetch_row(res);

    return (password == row[0]);
}

// 辅助函数：创建 double 类型参数绑定
MYSQL_BIND create_double_bind(double value) {
    MYSQL_BIND bind;
    memset(&bind, 0, sizeof(bind));
    bind.buffer_type = MYSQL_TYPE_DOUBLE;
    bind.buffer = (void*)&value;
    bind.buffer_length = sizeof(double);
    bind.is_null = 0;
    bind.length = nullptr;
    return bind;
}

// 辅助函数：创建 string 类型参数绑定
MYSQL_BIND create_string_bind(const std::string& value) {
    MYSQL_BIND bind;
    memset(&bind, 0, sizeof(bind));
    bind.buffer_type = MYSQL_TYPE_STRING;
    bind.buffer = (void*)value.c_str();
    bind.buffer_length = value.size();
    bind.is_null = 0;
    bind.length = nullptr;  // 对于固定长度字符串可设为 nullptr
    return bind;
}

// 创建 DATETIME 类型绑定
MYSQL_BIND create_datetime_bind(const MYSQL_TIME& value) {
    MYSQL_BIND bind;
    memset(&bind, 0, sizeof(bind));
    bind.buffer_type = MYSQL_TYPE_DATETIME;
    bind.buffer = (void*)&value;
    bind.buffer_length = sizeof(MYSQL_TIME);
    bind.is_null = 0;
    return bind;
}

std::string getCurrentTime() {
    auto now = std::chrono::system_clock::now();
    auto now_c = std::chrono::system_clock::to_time_t(now);
    std::tm now_tm;
    localtime_s(&now_tm, &now_c);  // 使用本地时间

    std::ostringstream oss;
    oss << std::put_time(&now_tm, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

int main()
{

    // 初始化 HTTP 服务器
    httplib::Server svr;

    // 配置静态文件目录（假设 login.html 放在 static 目录下）
    svr.set_base_dir("./static");

    svr.set_mount_point("/avatars", "./avatars");

    // 处理根路径（返回登录页面）
svr.Get("/", [](const httplib::Request& req, httplib::Response& res) {
        std::ifstream in("static/login.html");
        std::string html((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        res.set_content(html, "text/html; charset=utf-8");
    });

    // 处理登录接口（POST）
svr.Post("/login", [](const httplib::Request& req, httplib::Response& res) {
        try {
            // 检查 Content-Type 是否为 JSON
             auto content_type_it = req.headers.find("Content-Type");
            if (content_type_it == req.headers.end() || 
                content_type_it->second != "application/json") {
                throw std::runtime_error("Invalid Content-Type");
            }

            // 解析 JSON Body
            json j = json::parse(req.body);
            json result;
            std::string identifier  = j.at("identifier").get<std::string>();
            std::string password = j.at("password").get<std::string>();

            // 连接数据库并验证用户
            MYSQL* conn = connectDB();

            if (identifier.empty() || password.empty()) {
            result["status"] = "error";
            result["message"] = "账号和密码为必填项";
            res.set_content(result.dump(), "application/json");
            return;
        }

            if (mysql_ping(conn) != 0) { 
                std::cerr << "Connection lost: " << mysql_error(conn) << std::endl;
            }
            if (!conn) {
                res.set_content(u8"{\"success\": false, \"message\": \"数据库连接失败\"}", "application/json");
                return;
            }

            bool isValid = validateUser(identifier, password, conn);
            MYSQL_ROW row;
            if(isValid)
            {
                 MYSQL_STMT* stmt = mysql_stmt_init(conn);
                 if (!stmt) throw std::runtime_error(mysql_error(conn));

                 string query;

                 for(int i=0;i<identifier.length();i++)
                 {
                     if(identifier[i]=='@')
                     {
                         query="SELECT username, email, phone FROM users WHERE email='" + identifier + "'" ;
                         break;
                     }
                     else
                     {
                         query="SELECT username, email, phone FROM users WHERE phone='" + identifier + "'" ;
                     }
                 }
                 std::cout<<query;
                //if (mysql_stmt_prepare(stmt, query.c_str(), query.length())) {
                //     throw std::runtime_error(mysql_stmt_error(stmt));
                // }
                 if (mysql_ping(conn) != 0) {
                     std::cerr << "Connection lost: " << mysql_error(conn) << std::endl;
         
                 }
         
                 // 获取结果
                 json result = executeSelectQuery(conn, query);
         
                 if (mysql_query(conn, query.c_str())) {
                     std::cerr << u8"执行失败: " << mysql_error(conn) << std::endl;
                     mysql_close(conn);
                     return;
                 }
         
                 MYSQL_RES* res_return = mysql_store_result(conn);
                 if (!res_return) {
                     std::cerr << u8"获取结果集失败: " << mysql_error(conn) << std::endl;
                     mysql_close(conn);
                     return;
                 }
         
                 // 清理资源
                 mysql_stmt_close(stmt);
                 mysql_close(conn);

                 row = mysql_fetch_row(res_return);
            }

            
            // 返回结果
            if(isValid)
            {
                json response = {
                    {"success", true},
                    {"message", "登录成功"},
                    {"data", {
                       {"username", row[0]},
                       {"email", row[1]},
                       {"phone", row[2]}
                    }}
                };
                res.set_content(response.dump(), "application/json");
            }
            else{
                json resp={("message","")};
                res.status=400;
                res.set_content(resp.dump(), "application/json");
            }
            
        } catch (const std::exception& e) {
            res.status = 400;
            res.set_content(u8"{\"success\": false, \"message\": \"" + std::string(e.what()) + "\"}", "application/json");
        }
    });

svr.Post("/svc_create", [](const httplib::Request& req, httplib::Response& res) {
    try {
        json j = json::parse(req.body);
        
        // 参数校验
        std::vector<std::string> required = {
            "service_name", "service_type", "provider_name",
            "description", "price", "available_time"
        };
        for (auto& param : required) {
            if (!j.contains(param)) {
                throw std::runtime_error("Missing parameter: " + param);
            }
        }

        // 时间格式验证与转换
        std::tm tm = {};
        std::istringstream ss(j["available_time"].get<std::string>());
        if (!(ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M"))) {
            throw std::runtime_error("Invalid time format");
        }
        auto time = std::mktime(&tm);
        std::ostringstream oss;
oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
std::string formatted_time = oss.str();
        // 数据库插入操作
        MYSQL* conn = connectDB();
        MYSQL_STMT* stmt = mysql_stmt_init(conn);
        const char* query = "INSERT INTO services "
                           "(service_name, service_type, provider_name, description, price, available_time) "
                           "VALUES (?, ?, ?, ?, ?, ?)";
        mysql_stmt_prepare(stmt, query, strlen(query));

        MYSQL_BIND params[6];
        memset(params, 0, sizeof(params));

        // 绑定参数（带类型验证）
        params[0].buffer_type = MYSQL_TYPE_STRING;
        params[0].buffer = (void*)j["service_name"].get<std::string>().c_str();
        params[0].buffer_length = j["service_name"].get<std::string>().size();

        params[1].buffer_type = MYSQL_TYPE_STRING;
        params[1].buffer = (void*)j["service_type"].get<std::string>().c_str();
        params[1].buffer_length = j["service_type"].get<std::string>().size();

        params[2].buffer_type = MYSQL_TYPE_STRING;
        params[2].buffer = (void*)j["provider_name"].get<std::string>().c_str();
        params[2].buffer_length = j["provider_name"].get<std::string>().size();

        params[3].buffer_type = MYSQL_TYPE_STRING;
        params[3].buffer = (void*)j["description"].get<std::string>().c_str();
        params[3].buffer_length = j["description"].get<std::string>().size();

        params[4].buffer_type = MYSQL_TYPE_DOUBLE;
        params[4].buffer = (void*)&j["price"];
        params[4].buffer_length = sizeof(double);

        params[5].buffer_type = MYSQL_TYPE_DATETIME;
        params[5].buffer = (void*)&time;
        params[5].buffer_length = sizeof(time_t);

        mysql_stmt_bind_param(stmt, params);
        mysql_stmt_execute(stmt);
        int affected_rows = mysql_stmt_affected_rows(stmt);

        mysql_stmt_close(stmt);
        mysql_close(conn);

        res.set_content(affected_rows > 0 
            ? u8"{\"success\": true, \"message\": \"服务创建成功\"}"
            : u8"{\"success\": false, \"message\": \"服务创建失败\"}",
            "application/json");

    } catch (const std::exception& e) {
        res.status = 400;
        res.set_content(u8"{\"success\": false, \"message\": \"" + std::string(e.what()) + "\"}", "application/json");
    }
});

// 在main函数中添加新路由（在现有路由之后添加）
svr.Post("/retrieve", [](const httplib::Request& req, httplib::Response& res) {
    try {
        // 检查 Content-Type 是否为 JSON
        if (req.headers.find("Content-Type")->second != "application/json") {
            throw std::runtime_error("Invalid Content-Type");
        }

        // 解析 JSON Body
        json j = json::parse(req.body);

        // 参数校验
        std::vector<std::string> required = {"username", "newPassword", "email", "phone"};
        for (auto& param : required) {
            if (!j.contains(param)) {
                throw std::runtime_error("Missing parameter: " + param);
            }
        }

        std::string username = j["username"];
        std::string password = j["newPassword"];
        std::string email = j["email"];
        std::string phone = j["phone"];

        // 连接数据库
        MYSQL* conn = connectDB();
        if (!conn) throw std::runtime_error("Database connection failed");

        // 检查用户是否存在
        MYSQL_STMT* stmt = mysql_stmt_init(conn);
        if (!stmt) throw std::runtime_error(mysql_error(conn));

        std::string sql_check = "SELECT password FROM users WHERE username= '"
        +username+ "' AND email= '"
        +email+ "' AND phone= '"
        +phone+"'";
        std::cout<<sql_check<<endl;
        if (mysql_stmt_prepare(stmt, sql_check.c_str(), sql_check.length())) {
            throw std::runtime_error(mysql_stmt_error(stmt));
        }

        if (mysql_ping(conn) != 0) {
            std::cerr << "Connection lost: " << mysql_error(conn) << std::endl;
        }

        json result = executeSelectQuery(conn, sql_check);

        if (mysql_query(conn, sql_check.c_str())) {
            std::cerr << u8"执行失败: " << mysql_error(conn) << std::endl;
            mysql_close(conn);
            return;
        }

        MYSQL_RES* res_check = mysql_store_result(conn);
        if (!res_check) {
            std::cerr << u8"获取结果集失败: " << mysql_error(conn) << std::endl;
            mysql_close(conn);
            return;
        }

        mysql_stmt_close(stmt);
        
        MYSQL_ROW row= mysql_fetch_row(res_check);


        if (row == 0) {
            json resp={("message","用户信息有误")};
            res.status=400;
            res.set_content(resp.dump(), "application/json");
            return;
        }

        // 更新密码
        stmt = mysql_stmt_init(conn);
        if (!stmt) throw std::runtime_error(mysql_error(conn));
         
        std::string sql_update = "UPDATE users SET password= '"+password+"' WHERE email= '"+email+"'";
        std::cout<<sql_update<<endl;
        if (mysql_stmt_prepare(stmt, sql_update.c_str(), sql_update.length())) {
            throw std::runtime_error(mysql_stmt_error(stmt));
        }

        if (mysql_ping(conn) != 0) {
            std::cerr << "Connection lost: " << mysql_error(conn) << std::endl;
        }

        json result_update = executeSelectQuery(conn, sql_update);

        if (mysql_query(conn, sql_check.c_str())) {
            std::cerr << u8"执行失败: " << mysql_error(conn) << std::endl;
            mysql_close(conn);
            return;
        }

        MYSQL_RES* res_update = mysql_store_result(conn);
        if (!res_update) {
            std::cerr << u8"获取结果集失败: " << mysql_error(conn) << std::endl;
            mysql_close(conn);
            return;
        }

         MYSQL_ROW row_update= mysql_fetch_row(res_update);

        int affected_rows = mysql_affected_rows(conn);
        mysql_stmt_close(stmt);
        mysql_close(conn);

        res.set_content(affected_rows > 0 
            ? u8"{\"success\": true, \"message\": \"密码重置成功\"}"
            : u8"{\"success\": false, \"message\": \"密码更新失败\"}",
            "application/json");

    } catch (const std::exception& e) {
        res.status = 400;
        res.set_content(u8"{\"success\": false, \"message\": \"" + std::string(e.what()) + "\"}", "application/json");
    }
});

    // 在main函数中添加注册路由（在/login路由之后添加）
svr.Post("/register", [](const httplib::Request& req, httplib::Response& res) {
    try {
        // 检查 Content-Type 是否为 JSON
        if (req.headers.find("Content-Type")->second != "application/json") {
            throw std::runtime_error("Invalid Content-Type");
        }

        // 解析 JSON Body
        json j = json::parse(req.body);

        std::vector<std::string> required = {"username", "password", "email", "user_type", "phone"};
        for (auto& param : required) {
            if (!j.contains(param)) {
                throw std::runtime_error("Missing parameter: " + param);
            }
        }

        std::string username = j.at("username").get<std::string>();
        std::string password = j.at("password").get<std::string>();
        std::string email = j.at("email").get<std::string>();
        std::string user_type = j.at("user_type").get<std::string>();
        std::string phone  = j.at("phone").get<std::string>();
        
        if (username.empty() || email.empty() || phone.empty() || 
            user_type.empty() || password.empty()) {
            
            res.set_content("{\"status\":\"error\",\"message\":\"所有字段必填\"}", "application/json");
            return;
        }

        // 连接数据库
        MYSQL* conn = connectDB();
        if (!conn) throw std::runtime_error("数据库连接失败");

        MYSQL_STMT* stmt_email = mysql_stmt_init(conn);
        if(!stmt_email)throw std::runtime_error(mysql_error(conn));
        string sql_email = "SELECT COUNT(*) FROM users WHERE email = '"+email+"'";
        std::cout<<sql_email<<endl;
        if(mysql_stmt_prepare(stmt_email, sql_email.c_str(), sql_email.length()))
        {
            throw std::runtime_error(mysql_stmt_error(stmt_email));
        }

        std::string current_time = getCurrentTime();

        if(mysql_ping(conn)!=0)
        {
            std::cerr<<"Connection Lost: "<<mysql_error(conn)<<std::endl;
        }

        json result_email=executeSelectQuery(conn,sql_email);

        if (mysql_query(conn, sql_email.c_str())) {
            std::cerr << u8"执行失败: " << mysql_error(conn) << std::endl;
            mysql_close(conn);
            return;
        }

         MYSQL_RES* res_email = mysql_store_result(conn);
        if (!res_email) {
            std::cerr << u8"获取结果集失败: " << mysql_error(conn) << std::endl;
            mysql_close(conn);
            return;
        }

        mysql_stmt_close(stmt_email);

        MYSQL_ROW row_email;
        row_email = mysql_fetch_row(res_email);
        long email_exists=atoll(row_email[0]);
        //MYSQL_BIND bind_email = {0};
        //bind_email.buffer_type = MYSQL_TYPE_STRING;
        //bind_email.buffer = (void*)email.c_str();
        //bind_email.buffer_length = email.size();
        
        //mysql_stmt_bind_param(stmt_email, &bind_email);
        //mysql_stmt_execute(stmt_email);
        //MYSQL_BIND result_bind_email = {};
        //result_bind_email.buffer_type = MYSQL_TYPE_LONG;
        //mysql_stmt_bind_result(stmt_email, &result_bind_email);
        //mysql_stmt_fetch(stmt_email);
        //long email_exists = *(long*)result_bind_email.buffer;
        //mysql_stmt_close(stmt_email);

        // 手机号存在性检查
        MYSQL_STMT* stmt_phone = mysql_stmt_init(conn);
        if(!stmt_phone)throw std::runtime_error(mysql_error(conn));
        string sql_phone = "SELECT COUNT(*) FROM users WHERE phone = '"+phone+"'";
        std::cout<<sql_phone<<endl;
        if(mysql_stmt_prepare(stmt_phone, sql_phone.c_str(), sql_phone.length()))
        {
            throw std::runtime_error(mysql_stmt_error(stmt_phone));
        }

        if(mysql_ping(conn)!=0)
        {
            std::cerr<<"Connection Lost: "<<mysql_error(conn)<<std::endl;
        }

        json result_phone=executeSelectQuery(conn,sql_phone);

        if (mysql_query(conn, sql_phone.c_str())) {
            std::cerr << u8"执行失败: " << mysql_error(conn) << std::endl;
            mysql_close(conn);
            return;
        }

         MYSQL_RES* res_phone = mysql_store_result(conn);
        if (!res_phone) {
            std::cerr << u8"获取结果集失败: " << mysql_error(conn) << std::endl;
            mysql_close(conn);
            return;
        }

        mysql_stmt_close(stmt_phone);

        MYSQL_ROW row_phone;
        row_phone = mysql_fetch_row(res_phone);
        long phone_exists=atoll(row_phone[0]);

        if (email_exists > 0 && phone_exists > 0) {
            //std::cout<<"邮箱和手机号均已存在"<<endl;
            json resp={("message","邮箱和手机号均已存在")};
            res.status=400;
            res.set_content(resp.dump(), "application/json");
        } else if (email_exists > 0) {
            json resp={("message","邮箱已存在")};
            res.status=400;
            res.set_content(resp.dump(), "application/json");
        } else if (phone_exists > 0) {
            json resp={("message","手机号已存在")};
            res.status=400;
            res.set_content(resp.dump(), "application/json");
        } else {

        // 插入新用户到数据库
        MYSQL_STMT* stmt = mysql_stmt_init(conn);
        if (!stmt) throw std::runtime_error("mysql_stmt_init失败");

        string sql = "INSERT INTO users (username, email, phone, user_type, password, registration_time) "  // 添加created_at字段
                     "VALUES ('" + username + "','" + email + "','" + phone + "','" 
                     + user_type + "','" + password + "', '" + current_time + "')";
                         std::cout<<sql<<endl;
        if (mysql_stmt_prepare(stmt, sql.c_str(), sql.length())) {
            //res.set_status(500);
            res.set_content("{\"status\":\"error\",\"message\":\"数据库错误\"}", "application/json");
            return;
        }

        json result=executeSelectQuery(conn,sql);

        //MYSQL_BIND bind[5] = {0};
        //bind[0].buffer_type = MYSQL_TYPE_STRING;
        //bind[0].buffer = (void*)username.c_str();
        //bind[0].buffer_length = username.size();

        //bind[1].buffer_type = MYSQL_TYPE_STRING;
        //bind[1].buffer = (void*)email.c_str();
        //bind[1].buffer_length = email.size();

        //bind[2].buffer_type = MYSQL_TYPE_STRING;
        //bind[2].buffer = (void*)phone.c_str();
        //bind[2].buffer_length = phone.size();

        //bind[3].buffer_type = MYSQL_TYPE_ENUM;
        //bind[3].buffer = (void*)user_type.c_str();

        //bind[4].buffer_type = MYSQL_TYPE_STRING;
        //bind[4].buffer = (void*)password.c_str();
        //bind[4].buffer_length = password.size();

        //if (mysql_stmt_bind_param(stmt, bind)) {
        //    //res.set_status(500);
        //    res.set_content("{\"status\":\"error\",\"message\":\"参数绑定失败\"}", "application/json");
        //    return;
        //}

        //if (mysql_stmt_execute(stmt)) {
        //    //res.set_status(500);
        //    res.set_content("{\"status\":\"error\",\"message\":\"注册失败\"}", "application/json");
        //    return;
        //}

        if (mysql_query(conn, sql.c_str())) {
            std::cerr << u8"执行失败: " << mysql_error(conn) << std::endl;
            mysql_close(conn);
            return;
        }

        MYSQL_RES* res_register = mysql_store_result(conn);
        if (!res_register) {
            std::cerr << u8"获取结果集失败: " << mysql_error(conn) << std::endl;
            mysql_close(conn);
            return;
        }

        int affected_rows = mysql_stmt_affected_rows(stmt);

        mysql_stmt_close(stmt);
        mysql_close(conn);

            if (affected_rows > 0) {
                res.set_content(u8"{\"status\":\"success\",\"message\":\"注册成功\"}", "application/json");
            } else {
                res.set_content(u8"{\"status\":\"error\",\"message\":\"注册失败，请重试\"}", "application/json");
            }
    }
    } catch (const std::exception& e) {
        res.status = 400;
        res.set_content(u8"{\"success\": false, \"message\": \"" + std::string(e.what()) + "\"}", "application/json");
    }
});

svr.Get("/register", [](const httplib::Request&, httplib::Response& res) {
    std::ifstream in("static/register.html");
    std::string html((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    res.set_content(html, "text/html; charset=utf-8");
});

        // 在main函数中，将以下代码添加到现有路由之后，但在svr.listen()之前
svr.Get("/dashboard", [](const httplib::Request&, httplib::Response& res) {
        std::ifstream in("static/dashboard.html");
        std::string html((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        res.set_content(html, "text/html; charset=utf-8");
});

// 在main函数中添加新路由
svr.Get("/userProfile", [](const httplib::Request& req, httplib::Response& res) {
    try {
        //res.set_redirect("/userProfile");
        // 获取identify参数
        
        std::string identify = req.get_param_value("phone");
        if (identify.empty()) {
            res.status = 400;
            res.set_content(u8"{\"success\": false, \"message\": \"缺少identify参数\"}", "application/json");
            return;
        }

        // 连接数据库
        MYSQL* conn = connectDB();
        if (!conn) {
            res.status = 500;
            res.set_content(u8"{\"success\": false, \"message\": \"数据库连接失败\"}", "application/json");
            return;
        }
        

        // 构造查询语句
        std::string sql = "SELECT username, email, phone, gender, birth_date, bio, registration_time, image_url, subscription_count, preferred_time, preferred_services, service_area, address, service_time FROM users WHERE ";
        if (identify.find('@') != std::string::npos) {
            sql += "email = '"+identify+"'";
        } else {
            sql += "phone = '"+identify+"'";
        }

        cout<<sql<<endl;

        if (mysql_ping(conn) != 0) {
            std::cerr << "Connection lost: " << mysql_error(conn) << std::endl;
        }

        json result = executeSelectQuery(conn, sql);

        if (mysql_query(conn, sql.c_str())) {
            std::cerr << u8"执行失败: " << mysql_error(conn) << std::endl;
            mysql_close(conn);
            return;
        }

         MYSQL_RES* res_return = mysql_store_result(conn);
        if (!res_return) {
            std::cerr << u8"获取结果集失败: " << mysql_error(conn) << std::endl;
            mysql_close(conn);
            return;
        }

        MYSQL_ROW row;
        row = mysql_fetch_row(res_return);
       
       // res.set_content(response.dump(), "application/json");

    
        // 清理资源
        mysql_close(conn);
        json response = {
                    {"success", true},
                    {"message", "登录成功"},
                    {"userData", {
                       {"name", row[0]},
                       {"email", row[1]},
                       {"phone", row[2]},
                       {"gender",row[3]?row[3]:""},
                       {"birthday",row[4]?row[4]:""},
                       {"bio",row[5]?row[5]:""},
                       {"registerTime",row[6]},
                       {"avatarUrl",row[7]?row[7]:""},
                       {"bookingCount",row[8]},
                       {"preferences",{
                        {"preferredTime",row[9]?row[9]:""},
                        {"categories",row[10]?row[10]:"[,,]"}
                       }
                       },
                       {"specialty",row[11]?row[11]:""},
                       {"address",row[12]?row[12]:""},
                       {"businessHours",row[13]?row[13]:""},
                       {"verified",true}
                    }}
                };
    res.set_content(response.dump(), "application/json");
    } catch (const std::exception& e) {
        res.status = 500;
        res.set_content(u8"{\"success\": false, \"message\": \"" + std::string(e.what()) + "\"}", "application/json");
    }
});

svr.Get("/userHome/getUserData", [](const httplib::Request& req, httplib::Response& res) {
    try {
        //res.set_redirect("/userProfile");
        // 获取identify参数
        
        std::string identify = req.get_param_value("phone");
        if (identify.empty()) {
            res.status = 400;
            res.set_content(u8"{\"success\": false, \"message\": \"缺少identify参数\"}", "application/json");
            return;
        }

        // 连接数据库
        MYSQL* conn = connectDB();
        if (!conn) {
            res.status = 500;
            res.set_content(u8"{\"success\": false, \"message\": \"数据库连接失败\"}", "application/json");
            return;
        }
        

        // 构造查询语句
        std::string sql = "SELECT * FROM users WHERE ";
        if (identify.find('@') != std::string::npos) {
            sql += "email = '"+identify+"'";
        } else {
            sql += "phone = '"+identify+"'";
        }

        std::cout<<sql<<endl;

        if (mysql_ping(conn) != 0) {
            std::cerr << "Connection lost: " << mysql_error(conn) << std::endl;
        }

        json result = executeSelectQuery(conn, sql);

        if (mysql_query(conn, sql.c_str())) {
            std::cerr << u8"执行失败: " << mysql_error(conn) << std::endl;
            mysql_close(conn);
            return;
        }

         MYSQL_RES* res_return = mysql_store_result(conn);
        if (!res_return) {
            std::cerr << u8"获取结果集失败: " << mysql_error(conn) << std::endl;
            mysql_close(conn);
            return;
        }

        MYSQL_ROW row;
        row = mysql_fetch_row(res_return);

        string user_id=row[0];

        //查服务
        std::string query_appointments= "SELECT * FROM appointment_records WHERE user_id = '"+user_id+"'";
        std::cout<<query_appointments<<endl;
        if (mysql_ping(conn) != 0) {
            std::cerr << "Connection lost: " << mysql_error(conn) << std::endl;
        }
        json result_appointments = executeSelectQuery(conn, query_appointments);
        if (mysql_query(conn, query_appointments.c_str())) {
            std::cerr << u8"执行失败: " << mysql_error(conn) << std::endl;
            mysql_close(conn);
            return;
        }

         MYSQL_RES* res_appointments = mysql_store_result(conn);
        if (!res_appointments) {
            std::cerr << u8"获取结果集失败: " << mysql_error(conn) << std::endl;
            mysql_close(conn);
            return;
        }
        MYSQL_ROW row_appointments;
        int pendingAppointments=0;
        int pendingConfirmations=0;
        int completedAppointments=0;
        int pendingReviews=0;
        std::string query_update_status = "UPDATE appointment_records "
                      "SET status = 'completed' "
                      "WHERE appointment_date < NOW() AND user_id = '"+user_id+"'";
        json result_update_status = executeSelectQuery(conn, query_update_status);
        if (mysql_query(conn, query_update_status.c_str())) {
            std::cerr << u8"执行失败: " << mysql_error(conn) << std::endl;
            mysql_close(conn);
            return;
        }
        
        std::string query_pendingAppointments = "SELECT count(*) FROM appointment_records WHERE DATE(appointment_date) = CURDATE() AND TIME(appointment_date) > CURTIME() AND status = 'pending' AND user_id = '"+user_id+"'";

        json result_pendingAppointments = executeSelectQuery(conn, query_pendingAppointments);
        if (mysql_query(conn, query_pendingAppointments.c_str())) {
            std::cerr << u8"执行失败: " << mysql_error(conn) << std::endl;
            mysql_close(conn);
            return;
        }
        MYSQL_RES* res_pendingAppointments = mysql_store_result(conn);
        if (!res_pendingAppointments) {
            std::cerr << u8"获取结果集失败: " << mysql_error(conn) << std::endl;
            mysql_close(conn);
            return;
        }
        MYSQL_ROW row_pendingAppointments= mysql_fetch_row(res_pendingAppointments);
        pendingAppointments+=atoll(row_pendingAppointments[0]);
        
        std::string query_pendingConfirmations = "SELECT count(*) FROM appointment_records WHERE status = 'pending' AND user_id = '"+user_id+"'";
        std::cout<<query_pendingConfirmations<<endl;
        json result_pendingConfirmations = executeSelectQuery(conn, query_pendingConfirmations);
        if (mysql_query(conn, query_pendingConfirmations.c_str())) {
            std::cerr << u8"执行失败: " << mysql_error(conn) << std::endl;
            mysql_close(conn);
            return;
        }
        MYSQL_RES* res_pendingConfirmations = mysql_store_result(conn);
        if (!res_pendingConfirmations) {
            std::cerr << u8"获取结果集失败: " << mysql_error(conn) << std::endl;
            mysql_close(conn);
            return;
        }
        MYSQL_ROW row_pendingConfirmations= mysql_fetch_row(res_pendingConfirmations);
        pendingConfirmations+=atoll(row_pendingConfirmations[0]);

        std::string query_completedAppointments = "SELECT count(*) FROM appointment_records WHERE status = 'completed' AND user_id = '"+user_id+"'";
        std::cout<<query_completedAppointments<<endl;
        json result_completedAppointments = executeSelectQuery(conn, query_completedAppointments);
        if (mysql_query(conn, query_completedAppointments.c_str())) {
            std::cerr << u8"执行失败: " << mysql_error(conn) << std::endl;
            mysql_close(conn);
            return;
        }
        MYSQL_RES* res_completedAppointments = mysql_store_result(conn);
        if (!res_completedAppointments) {
            std::cerr << u8"获取结果集失败: " << mysql_error(conn) << std::endl;
            mysql_close(conn);
            return;
        }
        MYSQL_ROW row_completedAppointments= mysql_fetch_row(res_completedAppointments);
        completedAppointments+=atoll(row_completedAppointments[0]);

        std::string query_pendingReviews = "SELECT count(*) FROM appointment_records WHERE status = 'completed' AND rating IS NULL AND user_id = '"+user_id+"'";
        std::cout<<query_pendingReviews<<endl;
        json result_pendingReviews = executeSelectQuery(conn, query_pendingReviews);
        if (mysql_query(conn, query_pendingReviews.c_str())) {
            std::cerr << u8"执行失败: " << mysql_error(conn) << std::endl;
            mysql_close(conn);
            return;
        }
        MYSQL_RES* res_pendingReviews = mysql_store_result(conn);
        if (!res_pendingReviews) {
            std::cerr << u8"获取结果集失败: " << mysql_error(conn) << std::endl;
            mysql_close(conn);
            return;
        }
        MYSQL_ROW row_pendingReviews= mysql_fetch_row(res_pendingReviews);
        pendingReviews+=atoll(row_pendingReviews[0]);
       // res.set_content(response.dump(), "application/json");

    
        // 清理资源
        mysql_close(conn);
        json response = {
                    {"success", true},
                    {"message", "登录成功"},
                    
                };
                response["username"]= row[1];
                response["avatar"]= row[4];
                response["pendingAppointments"]= pendingAppointments;
                response["pendingConfirmations"]= pendingConfirmations;
                response["completedAppointments"]= completedAppointments;
                response["pendingReviews"]= pendingReviews;
    res.set_content(response.dump(), "application/json");
    } catch (const std::exception& e) {
        res.status = 500;
        res.set_content(u8"{\"success\": false, \"message\": \"" + std::string(e.what()) + "\"}", "application/json");
    }
});

svr.Get("/providerRecords", [](const httplib::Request& req, httplib::Response& res) {
    try {
        //res.set_redirect("/userProfile");
        // 获取identify参数
        
        std::string identify = req.get_param_value("phone");
        if (identify.empty()) {
            res.status = 400;
            res.set_content(u8"{\"success\": false, \"message\": \"缺少identify参数\"}", "application/json");
            return;
        }

        // 连接数据库
        MYSQL* conn = connectDB();
        if (!conn) {
            res.status = 500;
            res.set_content(u8"{\"success\": false, \"message\": \"数据库连接失败\"}", "application/json");
            return;
        }
        

        // 构造查询语句
        std::string query_provider = "SELECT username FROM users WHERE ";
        if (identify.find('@') != std::string::npos) {
            query_provider += "email = '"+identify+"'";
        } else {
            query_provider += "phone = '"+identify+"'";
        }

        std::cout<<query_provider<<endl;

        if (mysql_ping(conn) != 0) {
            std::cerr << "Connection lost: " << mysql_error(conn) << std::endl;
        }

        json result = executeSelectQuery(conn, query_provider);

        if (mysql_query(conn, query_provider.c_str())) {
            std::cerr << u8"执行失败: " << mysql_error(conn) << std::endl;
            mysql_close(conn);
            return;
        }

         MYSQL_RES* res_provider = mysql_store_result(conn);
        if (!res_provider) {
            std::cerr << u8"获取结果集失败: " << mysql_error(conn) << std::endl;
            mysql_close(conn);
            return;
        }

        MYSQL_ROW row_provider;
        row_provider = mysql_fetch_row(res_provider);

        string provider_name=row_provider[0];

        //查服务
        std::string query_service= "SELECT * FROM services WHERE provider_name = '"+provider_name+"'";
        std::cout<<query_service<<endl;
        if (mysql_ping(conn) != 0) {
            std::cerr << "Connection lost: " << mysql_error(conn) << std::endl;
        }
        json result_service = executeSelectQuery(conn, query_service);
        if (mysql_query(conn, query_service.c_str())) {
            std::cerr << u8"执行失败: " << mysql_error(conn) << std::endl;
            mysql_close(conn);
            return;
        }

         MYSQL_RES* res_service = mysql_store_result(conn);
        if (!res_service) {
            std::cerr << u8"获取结果集失败: " << mysql_error(conn) << std::endl;
            mysql_close(conn);
            return;
        }
        MYSQL_ROW row_service;
        json records;
        float totalIncome=0;
        int totalRecords=0;
        int totalRating=0;
        while(row_service=mysql_fetch_row(res_service))
        {
            
            string service_id=row_service[0];
            string query_appointmnet="SELECT * FROM appointment_records WHERE service_id = '"+service_id+"'";
            std::cout<<query_appointmnet<<endl;

            if (mysql_ping(conn) != 0) {
            std::cerr << "Connection lost: " << mysql_error(conn) << std::endl;
           }
           json result_appointment = executeSelectQuery(conn, query_appointmnet);
           if (mysql_query(conn, query_appointmnet.c_str())) {
               std::cerr << u8"执行失败: " << mysql_error(conn) << std::endl;
               mysql_close(conn);
               return;
           }
   
            MYSQL_RES* res_appointment = mysql_store_result(conn);
           if (!res_appointment) {
               std::cerr << u8"获取结果集失败: " << mysql_error(conn) << std::endl;
               mysql_close(conn);
               return;
           }
           MYSQL_ROW row_appointment;
           
            while(row_appointment=mysql_fetch_row(res_appointment))
            {
                totalRecords++;
                totalRating+=row_appointment[4]?stoi(row_appointment[4]):0;
                
             json item;
             item["id"]=row_appointment[0];
             item["serviceName"]=row_service[1];
             item["appointmentTime"]=row_appointment[7];
             item["price"]=row_service[5];
             item["status"]=row_appointment[3];
             
             string user_id=row_appointment[2];
             string query_user="SELECT * FROM users WHERE id = '"+user_id+"'";
             std::cout<<query_user<<endl;
             json result_user = executeSelectQuery(conn, query_user);
             if (mysql_query(conn, query_user.c_str())) {
                 std::cerr << u8"执行失败: " << mysql_error(conn) << std::endl;
                 mysql_close(conn);
                 return;
             }
     
             MYSQL_RES* res_user = mysql_store_result(conn);
             if (!res_user) {
                 std::cerr << u8"获取结果集失败: " << mysql_error(conn) << std::endl;
                 mysql_close(conn);
                 return;
             }
             MYSQL_ROW row_user=mysql_fetch_row(res_user);
             totalIncome+=stoi(row_service[5]);
             item["customerName"]=row_user[1];
             item["customerPhone"]=row_user[6];

             records.push_back(item);
            }
        }
        
        

    
        // 清理资源
        mysql_close(conn);
        json response = {
                    {"success", true},
                    {"message", "登录成功"},
                    
                };
        response["records"]=records;
        json summary;
        summary["totalRecords"]=totalRecords;
        summary["totalIncome"]=totalIncome;
        summary["averageRating"]=totalRecords?totalRating/totalRecords:0;
        response["summary"]=summary;

    res.set_content(response.dump(), "application/json");
    } catch (const std::exception& e) {
        res.status = 500;
        res.set_content(u8"{\"success\": false, \"message\": \"" + std::string(e.what()) + "\"}", "application/json");
    }
});

svr.Get("/providerHome", [](const httplib::Request& req, httplib::Response& res) {
    try {
        //res.set_redirect("/userProfile");
        // 获取identify参数
        
        std::string identify = req.get_param_value("identifier");
        if (identify.empty()) {
            res.status = 400;
            res.set_content(u8"{\"success\": false, \"message\": \"缺少identify参数\"}", "application/json");
            return;
        }

        // 连接数据库
        MYSQL* conn = connectDB();
        if (!conn) {
            res.status = 500;
            res.set_content(u8"{\"success\": false, \"message\": \"数据库连接失败\"}", "application/json");
            return;
        }
        

        // 构造查询语句
        std::string query_provider = "SELECT username FROM users WHERE ";
        if (identify.find('@') != std::string::npos) {
            query_provider += "email = '"+identify+"'";
        } else {
            query_provider += "phone = '"+identify+"'";
        }

        std::cout<<query_provider<<endl;

        if (mysql_ping(conn) != 0) {
            std::cerr << "Connection lost: " << mysql_error(conn) << std::endl;
        }

        json result = executeSelectQuery(conn, query_provider);

        if (mysql_query(conn, query_provider.c_str())) {
            std::cerr << u8"执行失败: " << mysql_error(conn) << std::endl;
            mysql_close(conn);
            return;
        }

         MYSQL_RES* res_provider = mysql_store_result(conn);
        if (!res_provider) {
            std::cerr << u8"获取结果集失败: " << mysql_error(conn) << std::endl;
            mysql_close(conn);
            return;
        }

        MYSQL_ROW row_provider;
        row_provider = mysql_fetch_row(res_provider);

        string provider_name=row_provider[0];

        //查服务
        std::string query_service= "SELECT * FROM services WHERE provider_name = '"+provider_name+"'";
        std::cout<<query_service<<endl;
        if (mysql_ping(conn) != 0) {
            std::cerr << "Connection lost: " << mysql_error(conn) << std::endl;
        }
        json result_service = executeSelectQuery(conn, query_service);
        if (mysql_query(conn, query_service.c_str())) {
            std::cerr << u8"执行失败: " << mysql_error(conn) << std::endl;
            mysql_close(conn);
            return;
        }

         MYSQL_RES* res_service = mysql_store_result(conn);
        if (!res_service) {
            std::cerr << u8"获取结果集失败: " << mysql_error(conn) << std::endl;
            mysql_close(conn);
            return;
        }
        MYSQL_ROW row_service;
        json records;
        float totalIncome=0;
        int totalRecords=0;
        int totalRating=0;
        int todayAppointments=0;
        int pendingAppointments=0;
        int activeServices=0;
        while(row_service=mysql_fetch_row(res_service))
        {
            
            string service_id=row_service[0];
            string query_appointmnet="SELECT * FROM appointment_records WHERE service_id = '"+service_id+"'";
            std::cout<<query_appointmnet<<endl;

            if (mysql_ping(conn) != 0) {
            std::cerr << "Connection lost: " << mysql_error(conn) << std::endl;
           }
           json result_appointment = executeSelectQuery(conn, query_appointmnet);
           if (mysql_query(conn, query_appointmnet.c_str())) {
               std::cerr << u8"执行失败: " << mysql_error(conn) << std::endl;
               mysql_close(conn);
               return;
           }
   
            MYSQL_RES* res_appointment = mysql_store_result(conn);
           if (!res_appointment) {
               std::cerr << u8"获取结果集失败: " << mysql_error(conn) << std::endl;
               mysql_close(conn);
               return;
           }
           MYSQL_ROW row_appointment;
           
            while(row_appointment=mysql_fetch_row(res_appointment))
            {
                totalRecords++;
                totalRating+=row_appointment[4]?stoi(row_appointment[4]):0;
                
             string user_id=row_appointment[1];
             string query_user="SELECT * FROM users WHERE id = '"+user_id+"'";
             std::cout<<query_user<<endl;
             json result_user = executeSelectQuery(conn, query_user);
             if (mysql_query(conn, query_user.c_str())) {
                 std::cerr << u8"执行失败: " << mysql_error(conn) << std::endl;
                 mysql_close(conn);
                 return;
             
             }
             string str=row_appointment[3];
             if(str=="pending")pendingAppointments++;
             MYSQL_RES* res_user = mysql_store_result(conn);
             if (!res_user) {
                 std::cerr << u8"获取结果集失败: " << mysql_error(conn) << std::endl;
                 mysql_close(conn);
                 return;
             }
             MYSQL_ROW row_user=mysql_fetch_row(res_user);
             totalIncome+=stoi(row_service[5]);

            }

            string query_appointmnet_today="SELECT COUNT(*) FROM appointment_records WHERE created_at >= CURDATE()"
  "AND created_at < CURDATE() + INTERVAL 1 DAY AND service_id = '"+service_id+"'";
            std::cout<<query_appointmnet_today<<endl;

            if (mysql_ping(conn) != 0) {
            std::cerr << "Connection lost: " << mysql_error(conn) << std::endl;
           }
           json result_appointment_today = executeSelectQuery(conn, query_appointmnet_today);
           if (mysql_query(conn, query_appointmnet_today.c_str())) {
               std::cerr << u8"执行失败: " << mysql_error(conn) << std::endl;
               mysql_close(conn);
               return;
           }
   
            MYSQL_RES* res_appointment_today = mysql_store_result(conn);
           if (!res_appointment_today) {
               std::cerr << u8"获取结果集失败: " << mysql_error(conn) << std::endl;
               mysql_close(conn);
               return;
           }
           MYSQL_ROW row_appointment_today=mysql_fetch_row(res_appointment_today);

           todayAppointments=stoi(row_appointment_today[0]);

           string query_appointmnet_active="SELECT COUNT(*) FROM appointment_records WHERE appointment_date >= CURDATE()"
  "AND appointment_date < CURDATE() + INTERVAL 1 DAY AND service_id = '"+service_id+"'";
            std::cout<<query_appointmnet_active<<endl;

            if (mysql_ping(conn) != 0) {
            std::cerr << "Connection lost: " << mysql_error(conn) << std::endl;
           }
           json result_appointment_active = executeSelectQuery(conn, query_appointmnet_active);
           if (mysql_query(conn, query_appointmnet_active.c_str())) {
               std::cerr << u8"执行失败: " << mysql_error(conn) << std::endl;
               mysql_close(conn);
               return;
           }
   
            MYSQL_RES* res_appointment_active = mysql_store_result(conn);
           if (!res_appointment_active) {
               std::cerr << u8"获取结果集失败: " << mysql_error(conn) << std::endl;
               mysql_close(conn);
               return;
           }
           MYSQL_ROW row_appointment_active=mysql_fetch_row(res_appointment_active);

           activeServices=stoi(row_appointment_active[0]);
        }
        
        

    
        // 清理资源
        mysql_close(conn);
        json response = {
                    {"success", true},
                    {"providerName", provider_name},
                    {"pendingAppointments", pendingAppointments},
                    {"newReviews", 0},
                    {"activeServices", activeServices},
                    {"todayCustomers", todayAppointments},
                    {"averageRating", totalRecords?totalRating/totalRecords:0},
                    {"todayAppointments", todayAppointments},
                    {"monthlyIncome", totalIncome},
                    {"pendingRequests", pendingAppointments}
                };
        response["records"]=records;
        

    res.set_content(response.dump(), "application/json");
    } catch (const std::exception& e) {
        res.status = 500;
        res.set_content(u8"{\"success\": false, \"message\": \"" + std::string(e.what()) + "\"}", "application/json");
    }
});

svr.Post("/uploadAvatar", [](const httplib::Request& req, httplib::Response& res) {
    
    //json j = json::parse(req.body);
    //std::string username = j.at("username").get<std::string>();
    //auto files = j.at("avator").get<std::string>();
    //std::string phone=j.at("phone").get<std::string>();
    // 1. 解析表单字段
    string phone;
    if(req.has_param("phone"))
    phone  = req.get_param_value("phone");

        

        // 2. 解析文件
        if (req.is_multipart_form_data()) {
            auto files = req.get_file_value("avatar"); // "avatar"为前端formData的key
            // files.filename: 原始文件名
            // files.content: 文件内容
            // files.content_type: 文件类型

            // 3. 构造保存路径（如 avatars/手机号_时间戳.扩展名）
            std::string ext = files.filename.substr(files.filename.find_last_of('.'));
            std::string save_path = "./avatars/" + phone + "_" + std::to_string(time(nullptr)) + ext;

            // 4. 保存文件
            std::ofstream ofs(save_path, std::ios::binary);
            ofs.write(files.content.c_str(), files.content.size());
            ofs.close();

            //保存头像
            MYSQL* conn = connectDB();
            if (!conn) throw std::runtime_error("数据库连接失败");

            std::string query = "UPDATE users SET image_url = '"+save_path+"' WHERE phone = '"+phone+"'";
            cout<<query<<endl;

            json result = executeSelectQuery(conn, query);

            if (mysql_query(conn, query.c_str())) {
            std::cerr << u8"执行失败: " << mysql_error(conn) << std::endl;
            mysql_close(conn);
            return;
            }

            // 5. 返回头像URL
            res.set_content(
                "{\"success\":true,\"avatarUrl\":\"" + save_path + "\"}",
                "application/json"
            );
        } else {
            res.set_content("{\"success\":false,\"message\":\"未收到文件\"}", "application/json");
        }
    });

svr.Post("/updateProfile", [](const httplib::Request& req, httplib::Response& res){
    auto content_type_it = req.headers.find("Content-Type");
            if (content_type_it == req.headers.end() || 
                content_type_it->second != "application/json") {
                throw std::runtime_error("Invalid Content-Type");
            }
    json j=json::parse(req.body);

    std::string identifier=j.at("identifier").get<std::string>();
    std::string phone=j.at("phone").get<std::string>();
    std::string email=j.at("email").get<std::string>();
    std::string name=j.at("name").get<std::string>();
    std::string gender=j.at("gender").get<std::string>();
    std::string birthday=j.at("birthday").get<std::string>();
    std::string bio=j.at("bio").get<std::string>();

    MYSQL* conn = connectDB();
    if (!conn) throw std::runtime_error("数据库连接失败");

    std::string query="UPDATE users "
                  "SET phone = '"+phone+"', "
                  "email = '"+email+"', "
                  "username = '"+name+"', "
                  "gender = '"+gender+"', "
                  "birth_date = '"+birthday+"', "
                  "bio = '"+bio;
                 

                  for(int i=0;i<identifier.size();i++)
                  {
                    if(identifier[i]=='@')
                    {
                        query+="' WHERE email= '"+identifier+"'";
                        break;
                    }
                    else
                    {
                        query="UPDATE users "
                              "SET phone = '"+phone+"', "
                               "email = '"+email+"', "
                              "username = '"+name+"', "
                              "gender = '"+gender+"', "
                               "birth_date = '"+birthday+"', "
                               "bio = '"+bio+"' WHERE phone= '"+identifier+"'";
                    }
                  }
    cout<<query;

    json result = executeSelectQuery(conn, query);

    if (mysql_query(conn, query.c_str())) {
        std::cerr << u8"执行失败: " << mysql_error(conn) << std::endl;
        mysql_close(conn);
        return;
    }

    mysql_close(conn);

    res.set_content(
        "{\"success\":true,\"message\":\"用户信息更新成功\"}",
        "application/json"
    );
});

svr.Post("/changePassword", [](const httplib::Request& req, httplib::Response& res){
    auto content_type_it = req.headers.find("Content-Type");
            if (content_type_it == req.headers.end() || 
                content_type_it->second != "application/json") {
                throw std::runtime_error("Invalid Content-Type");
            }
    json j=json::parse(req.body);

    std::string identifier=j.at("identifier").get<std::string>();
    std::string currentPassword=j.at("currentPassword").get<std::string>();
    std::string newPassword=j.at("newPassword").get<std::string>();
    

    MYSQL* conn = connectDB();
    if (!conn) throw std::runtime_error("数据库连接失败");

    std::string query_check="SELECT password FROM users";
                 

                  for(int i=0;i<identifier.size();i++)
                  {
                    if(identifier[i]=='@')
                    {
                        query_check+="' WHERE email= '"+identifier+"'";
                        break;
                    }
                    else
                    {
                        query_check="SELECT password FROM users WHERE phone= '"+identifier+"'";
                    }
                  }
                  cout<<query_check;

    json result_check = executeSelectQuery(conn, query_check);
    if (mysql_query(conn, query_check.c_str())) {
        std::cerr << u8"执行失败: " << mysql_error(conn) << std::endl;
        mysql_close(conn);
        return;
    }

    MYSQL_RES* res_check = mysql_store_result(conn);
    if (!res_check) {
            std::cerr << u8"获取结果集失败: " << mysql_error(conn) << std::endl;
            mysql_close(conn);
            return;
        }

    MYSQL_ROW row_check;
    row_check = mysql_fetch_row(res_check);
    if(row_check[0]!=currentPassword)
    {
        std::cerr << u8"密码错误: " << mysql_error(conn) << std::endl;
        mysql_close(conn);
        res.set_content(
        "{\"success\":false,\"message\":\"密码错误\"}",
        "application/json"
    );
    }
    
string query_change="UPDATE users SET password = '"+newPassword+"'";
    for(int i=0;i<identifier.size();i++)
    {
        if(identifier[i]=='@')
        {
            query_change+=" WHERE email= '"+identifier+"'";
            break;
        }
        else
        {
            query_change="UPDATE users SET password = '"+newPassword+"' WHERE phone= '"+identifier+"'";
        }
    }

    cout<<query_change<<endl;
    json result_change = executeSelectQuery(conn, query_change);

    if (mysql_query(conn, query_change.c_str())) {
            std::cerr << u8"执行失败: " << mysql_error(conn) << std::endl;
            mysql_close(conn);
            return;
    }
    mysql_close(conn);

    res.set_content(
        "{\"success\":true,\"message\":\"用户信息更新成功\"}",
        "application/json"
    );
});

svr.Post("/providerCreate", [](const httplib::Request& req, httplib::Response& res){
    //auto content_type_it = req.headers.find("Content-Type");
    //        if (content_type_it == req.headers.end() || 
    //            content_type_it->second != "application/json") {
    //            throw std::runtime_error("Invalid Content-Type");
    //        }
    //json j=json::parse(req.body);

    //std::string identifier=j.at("identifier").get<std::string>();
    //std::string serviceName=j.at("serviceName").get<std::string>();
    //std::string serviceProvider=j.at("serviceProvider").get<std::string>();
    //std::string serviceCategory=j.at("serviceCategory").get<std::string>();
    //std::string serviceDescription=j.at("serviceDescription").get<std::string>();
    //std::string startDate=j.at("startDate").get<std::string>();
    //std::string endDate=j.at("endDate").get<std::string>();
    //int duration=j.at("duration").get<int>();
    //int capacity=j.at("capacity").get<int>();
    //int basePrice=j.at("basePrice").get<int>();
    //int packageCount=j.at("packageCount").get<int>();
    //std::string pricingModel=j.at("pricingModel").get<std::string>();
    
    std::string identifier  = req.get_param_value("identifier");
    std::string serviceName  = req.get_param_value("serviceName");
    std::string serviceProvider  = req.get_param_value("serviceProvider");
    std::string serviceCategory  = req.get_param_value("serviceCategory");
    std::string serviceDescription  = req.get_param_value("serviceDescription");
    std::string startDate  = req.get_param_value("startDate");
    std::string endDate  = req.get_param_value("endDate");
    std::string duration  = req.get_param_value("duration");
    std::string capacity  = req.get_param_value("capacity");
    std::string basePrice  = req.get_param_value("basePrice");
    std::string packageCount  = req.get_param_value("packageCount");
    std::string pricingModel  = req.get_param_value("pricingModel");

    std::string save_path;

    if (req.is_multipart_form_data()) {
            auto files = req.get_file_value("serviceImage"); // "avatar"为前端formData的key
            // files.filename: 原始文件名
            // files.content: 文件内容
            // files.content_type: 文件类型

            // 3. 构造保存路径（如 avatars/手机号_时间戳.扩展名）
            std::string ext = files.filename.substr(files.filename.find_last_of('.'));
            save_path = "./avatars/" + identifier + "_" + std::to_string(time(nullptr)) + ext;

            // 4. 保存文件
            std::ofstream ofs(save_path, std::ios::binary);
            ofs.write(files.content.c_str(), files.content.size());
            ofs.close();

            //保存头像
            //MYSQL* conn = connectDB();
            //if (!conn) throw std::runtime_error("数据库连接失败");

            //std::string query = "UPDATE services SET picture = '"+save_path+"' WHERE phone = '"+identifier+"'";
            //cout<<query<<endl;

            //json result = executeSelectQuery(conn, query);

            //if (mysql_query(conn, query.c_str())) {
            //std::cerr << u8"执行失败: " << mysql_error(conn) << std::endl;
            //mysql_close(conn);
            //return;
            }

            // 5. 返回头像URL
            res.set_content(
                "{\"success\":true,\"avatarUrl\":\"" + save_path + "\"}",
                "application/json"
            );


    MYSQL* conn = connectDB();
    if (!conn) throw std::runtime_error("数据库连接失败");

    std::string query_service_provider="SELECT username FROM users WHERE phone = '"+identifier+"'";
    std::cout<<query_service_provider<<endl;
    json result_service_provider = executeSelectQuery(conn, query_service_provider);
    if (mysql_query(conn, query_service_provider.c_str())) {
            std::cerr << u8"执行失败: " << mysql_error(conn) << std::endl;
            mysql_close(conn);
            return;
    }
    MYSQL_RES* res_service_provider = mysql_store_result(conn);
    if (!res_service_provider) {
            std::cerr << u8"获取结果集失败: " << mysql_error(conn) << std::endl;
            mysql_close(conn);
            return;
    }
    MYSQL_ROW row_service_provider=mysql_fetch_row(res_service_provider);
    string provider_name=row_service_provider[0];
    float price=stoi(basePrice);
    if(pricingModel=="package")
    {
      price*=0.9*stoi(packageCount);
    }

    //服务商名字
    

    std::string query_insert = 
    "INSERT INTO services (service_name, provider_name, service_type, description, "
    "start_time, end_time, duration, capacity,picture,price) VALUES ("
    "'" + serviceName + "', "
    + "'" + provider_name + "', "
    + "'" + serviceCategory + "', "
    + "'" + serviceDescription + "', "
    + "'" + startDate + "', "
    + "'" + endDate + "', "
    + "'" +duration + "', "
    + "'" +capacity + "', "
    + "'" +save_path + "', "
    + "'" + std::to_string(price) + "')";
    std::cout<<query_insert<<endl;
    json result_insert = executeSelectQuery(conn, query_insert);
    
    mysql_close(conn);

    res.set_content(
        "{\"success\":true,\"message\":\"用户信息更新成功\"}",
        "application/json"
    );
});

svr.Post("/savePreferences", [](const httplib::Request& req, httplib::Response& res){
    auto content_type_it = req.headers.find("Content-Type");
            if (content_type_it == req.headers.end() || 
                content_type_it->second != "application/json") {
                throw std::runtime_error("Invalid Content-Type");
            }
    json j=json::parse(req.body);

    std::string identifier=j.at("identifier").get<std::string>();
    std::string preferredTime=j.at("preferredTime").get<std::string>();
    std::string categories=j.at("categories").get<std::string>();
    

    MYSQL* conn = connectDB();
    if (!conn) throw std::runtime_error("数据库连接失败");

    std::string query="UPDATE users "
                  "SET preferredTime = '"+preferredTime+"', "
                  "categories = '"+categories;
                  
                 

                  for(int i=0;i<identifier.size();i++)
                  {
                    if(identifier[i]=='@')
                    {
                        query+="' WHERE email= '"+identifier+"'";
                        break;
                    }
                    else
                    {
                        query="UPDATE users "
                  "SET preferred_time = '"+preferredTime+"', "
                  "preferred_services = '"+categories+"' WHERE phone= '"+identifier+"'";
                    }
                  }
    cout<<query;

    json result = executeSelectQuery(conn, query);

    if (mysql_query(conn, query.c_str())) {
        std::cerr << u8"执行失败: " << mysql_error(conn) << std::endl;
        mysql_close(conn);
        return;
    }

    mysql_close(conn);

    res.set_content(
        "{\"success\":true,\"message\":\"用户信息更新成功\"}",
        "application/json"
    );
});

svr.Get("/services", [](const httplib::Request& req, httplib::Response& res){
    
    std::string category = req.get_param_value("category");
    std::string location = req.get_param_value("location");
    std::string startDate = req.get_param_value("startDate");
    std::string endDate = req.get_param_value("endDate");
    std::string pageS = req.get_param_value("page");
    std::string pageSizeS = req.get_param_value("pageSize");
    
    int page=stoi(pageS);
    int pageSize=stoi(pageSizeS);

    MYSQL* conn = connectDB();
    if (!conn) throw std::runtime_error("数据库连接失败");

    std::string query = "SELECT * FROM services WHERE 1=1";
    
    if (!category.empty()) {
        query += " AND service_type = '" + category + "'";
    }
    if (!location.empty()) {
        query += " AND place = '" + location + "'";
    }
    if (!startDate.empty() && !endDate.empty()) {
    query += " AND (start_time <= '" + endDate + " 23:59:59' ";
    query += "   AND end_time >= '" + startDate + " 00:00:00')";
}
    
    // 添加排序和分页
    query += " ORDER BY id DESC LIMIT " + std::to_string(pageSize) 
             + " OFFSET " + std::to_string((page - 1) * pageSize);
    
    cout<<query;

    json result_get = executeSelectQuery(conn, query);

    if (mysql_query(conn, query.c_str())) {
        std::cerr << u8"执行失败: " << mysql_error(conn) << std::endl;
        mysql_close(conn);
        return;
    }

    MYSQL_RES* res_get = mysql_store_result(conn);
    if (!res_get) {
        std::cerr << u8"获取结果集失败: " << mysql_error(conn) << std::endl;
        mysql_close(conn);
        return;
    }
    json result;
    json servicesArray = json::array();

    MYSQL_ROW row;
     while ((row = mysql_fetch_row(res_get)))
     {
        string serviceId=row[0];
        std::string query_get_rating = "SELECT rating FROM appointment_records WHERE service_id = '" +serviceId + "'";

        cout<<query_get_rating<<endl;
        json result_get_rating=executeSelectQuery(conn,query_get_rating);
        if (mysql_query(conn, query_get_rating.c_str())) {
                std::cerr << u8"执行失败: " << mysql_error(conn) << std::endl;
                mysql_close(conn);
                return;
        }
        MYSQL_RES* res_get_rating = mysql_store_result(conn);
            if (!res_get_rating) {
                std::cerr << u8"获取结果集失败: " << mysql_error(conn) << std::endl;
                mysql_close(conn);
                return;
        }
        MYSQL_ROW row_get_rating;
        int count=0;
        double total_rating=0.0;
        while(row_get_rating=mysql_fetch_row(res_get_rating))
        {
            count++;
            total_rating+=atof(row_get_rating[0]);
            //if(row_get_rating[0])
            //{
            //    std::string query_update_rating = "UPDATE services SET rating = (SELECT AVG(rating) FROM appointment_records WHERE service_id = '" +serviceId + "') WHERE id = '" +serviceId + "'";
            //    cout<<query_update_rating<<endl;
            //    json result_update_rating=executeSelectQuery(conn,query_update_rating);
            //    if (mysql_query(conn, query_update_rating.c_str())) {
            //        std::cerr << u8"执行失败: " << mysql_error(conn) << std::endl;
            //        mysql_close(conn);
            //        return;
            //    }
            //}
        }
    
        double average_rating = count > 0 ? total_rating / count : 0.0;

        json item;
        item["id"] = row[0];
        item["title"] = row[1];
        item["category"] = row[2];
        item["image"] = row[10]?row[10]:" ";
        item["price"] = row[5];
        item["description"] = row[4];
        item["rating"]=average_rating;
        //// 其他字段可根据需要选择性添加
        
        servicesArray.push_back(item);
     }

    result["services"] = servicesArray;

    std::string query_total = "SELECT COUNT(*) AS total FROM services WHERE 1=1";

    if (!category.empty()) {
        query += " AND service_type = '" + category + "'";
    }
    if (!location.empty()) {
        query += " AND place = '" + location + "'";
    }
    if (!startDate.empty() && !endDate.empty()) {
        query += " AND (start_time <= '" + endDate + " 23:59:59'";
        query += " AND end_time >= '" + startDate + " 00:00:00')";
    }

    cout<<query_total<<endl;
    json result_total = executeSelectQuery(conn, query_total);
     if (mysql_query(conn, query_total.c_str())) {
            std::cerr << u8"执行失败: " << mysql_error(conn) << std::endl;
            mysql_close(conn);
            return;
    }

    MYSQL_RES* res_total = mysql_store_result(conn);
        if (!res_total) {
            std::cerr << u8"获取结果集失败: " << mysql_error(conn) << std::endl;
            mysql_close(conn);
            return;
    }

    MYSQL_ROW row_total;

    mysql_close(conn);
    row_total = mysql_fetch_row(res_total);
    long totalItems = row_total ? std::stol(row_total[0]) : 0;
    mysql_free_result(res_total);

    cout<<static_cast<int>(std::ceil(totalItems / static_cast<double>(pageSize)));

    result["pagination"] = {
        {"currentPage", pageS.empty() ? 1 : std::stoi(pageS)},
        {"totalPages", pageSizeS.empty() ? 10 : std::stoi(pageSizeS)},
        {"totalItems", totalItems}
    };

    res.set_content(
        result.dump(),
        "application/json"
    );
});

svr.Post("/bookings", [](const httplib::Request& req, httplib::Response& res){
    auto content_type_it = req.headers.find("Content-Type");
            if (content_type_it == req.headers.end() || 
                content_type_it->second != "application/json") {
                throw std::runtime_error("Invalid Content-Type");
            }
    json j=json::parse(req.body);

    std::string serviceName=j.at("serviceName").get<std::string>();
    std::string appointmentDate=j.at("appointmentDate").get<std::string>();
    std::string notes=j.at("notes").get<std::string>();
    std::string phone=j.at("identifier").get<std::string>();
    std::string couponId="";
    if(j.contains("couponId"))
    {
        if (!j["couponId"].is_null() && !j["couponId"].empty()) 
        {
            couponId=j.at("couponId").get<std::string>();
        }
    }
    

    MYSQL* conn = connectDB();
    if (!conn) throw std::runtime_error("数据库连接失败");


    //查找服务
    std::string query_service = "SELECT id FROM services WHERE service_name = '" +serviceName + "'";
    std::cout<<query_service<<endl;
    json result_services=executeSelectQuery(conn,query_service);

    if (mysql_query(conn, query_service.c_str())) {
            std::cerr << u8"执行失败: " << mysql_error(conn) << std::endl;
            mysql_close(conn);
            return;
    }

    MYSQL_RES* res_service = mysql_store_result(conn);
        if (!res_service) {
            std::cerr << u8"获取结果集失败: " << mysql_error(conn) << std::endl;
            mysql_close(conn);
            return;
        }
    MYSQL_ROW row_service= mysql_fetch_row(res_service);
    string id_service=row_service[0];

    //查找用户
    std::string query_user = "SELECT id FROM users WHERE phone = '" +phone + "'";
    std::cout<<query_user<<endl;
    json result_user=executeSelectQuery(conn,query_user);

    if (mysql_query(conn, query_user.c_str())) {
            std::cerr << u8"执行失败: " << mysql_error(conn) << std::endl;
            mysql_close(conn);
            return;
    }

    MYSQL_RES* res_user = mysql_store_result(conn);
        if (!res_user) {
            std::cerr << u8"获取结果集失败: " << mysql_error(conn) << std::endl;
            mysql_close(conn);
            return;
        }
    MYSQL_ROW row_user= mysql_fetch_row(res_user);
    string id_user=row_user[0];

    //插入预约
    std::string query_booking= "INSERT INTO appointment_records (service_id, user_id,appointment_date,notes) VALUES ('"
                               + id_service + "','" + id_user + "','"+appointmentDate+"','"+notes+"')";
    std::cout<<query_booking<<endl;
    json result_booking=executeSelectQuery(conn,query_user);
    if (mysql_query(conn, query_booking.c_str())) {
            std::cerr << u8"执行失败: " << mysql_error(conn) << std::endl;
            mysql_close(conn);
            return;
    }

if(couponId!="")
{
    std::string query_update_coupon ="UPDATE coupons SET status = 'USED' WHERE id = '" + couponId + "'";
    std::cout<<query_update_coupon<<endl;
    json result_update_coupon =executeSelectQuery(conn,query_update_coupon);
    if (mysql_query(conn, query_update_coupon.c_str())) {
            std::cerr << u8"执行失败: " << mysql_error(conn) << std::endl;
            mysql_close(conn);
            return;
    }
}

    res.set_content(u8"{\"success\": true, \"message\": \"预约成功\"}", "application/json");
});

svr.Get("/reviewsAll", [](const httplib::Request& req, httplib::Response& res) {
    try {
        //res.set_redirect("/userProfile");
        // 获取identify参数
        
        std::string identify = req.get_param_value("identifier");
        if (identify.empty()) {
            res.status = 400;
            res.set_content(u8"{\"success\": false, \"message\": \"缺少identify参数\"}", "application/json");
            return;
        }

        // 连接数据库
        MYSQL* conn = connectDB();
        if (!conn) {
            res.status = 500;
            res.set_content(u8"{\"success\": false, \"message\": \"数据库连接失败\"}", "application/json");
            return;
        }
        

        // 构造查询语句
        std::string sql = "SELECT id FROM users WHERE phone = '"+identify+"'";
        

        cout<<sql<<endl;

        json result = executeSelectQuery(conn, sql);

        if (mysql_query(conn, sql.c_str())) {
            std::cerr << u8"执行失败: " << mysql_error(conn) << std::endl;
            mysql_close(conn);
            return;
        }

         MYSQL_RES* res_return = mysql_store_result(conn);
        if (!res_return) {
            std::cerr << u8"获取结果集失败: " << mysql_error(conn) << std::endl;
            mysql_close(conn);
            return;
        }

        MYSQL_ROW row;
        row = mysql_fetch_row(res_return);
        string user_id=row[0];
        json pendingServices = json::array();
        json historyReviews= json::array();

        std::string query_services="SELECT * FROM appointment_records WHERE user_id = '"+user_id+"'";
        cout<<query_services<<endl;
        json result_services = executeSelectQuery(conn, query_services);

        if (mysql_query(conn, query_services.c_str())) {
            std::cerr << u8"执行失败: " << mysql_error(conn) << std::endl;
            mysql_close(conn);
            return;
        }

         MYSQL_RES* res_services = mysql_store_result(conn);
        if (!res_services) {
            std::cerr << u8"获取结果集失败: " << mysql_error(conn) << std::endl;
            mysql_close(conn);
            return;
        }
        MYSQL_ROW row_service;
        while(row_service=mysql_fetch_row(res_services))
        {
           if(!row_service[4])
           {
            json item;
            std::string service_id=row_service[1];
            std::string query_services_details= "SELECT * FROM services WHERE id = '"+service_id+"'";
            cout<<query_services_details<<endl;
            json result_services_details = executeSelectQuery(conn, query_services_details);
            if (mysql_query(conn, query_services_details.c_str())) {
                std::cerr << u8"执行失败: " << mysql_error(conn) << std::endl;
                mysql_close(conn);
                return;
            }
            MYSQL_RES* res_services_details = mysql_store_result(conn);
            if (!res_services_details) {
                std::cerr << u8"获取结果集失败: " << mysql_error(conn) << std::endl;
                mysql_close(conn);
                return;
            }
            MYSQL_ROW row_services_details;
            row_services_details = mysql_fetch_row(res_services_details);

            item["id"]=row_services_details[0];
            item["name"]=row_services_details[1];
            item["image"]=row_services_details[10];
            item["date"]=row_service[7];
            item["provider"]=row_services_details[3];
            item["price"]=row_services_details[5];
            item["location"]=row_services_details[9]?row_services_details[9]:"";
            pendingServices.push_back(item);
           }
           else
           {
            json item;
            std::string service_id=row_service[1];
            std::string query_services_details= "SELECT * FROM services WHERE id = '"+service_id+"'";
            cout<<query_services_details<<endl;
            json result_services_details = executeSelectQuery(conn, query_services_details);
            if (mysql_query(conn, query_services_details.c_str())) {
                std::cerr << u8"执行失败: " << mysql_error(conn) << std::endl;
                mysql_close(conn);
                return;
            }
            MYSQL_RES* res_services_details = mysql_store_result(conn);
            if (!res_services_details) {
                std::cerr << u8"获取结果集失败: " << mysql_error(conn) << std::endl;
                mysql_close(conn);
                return;
            }
            MYSQL_ROW row_services_details;
            row_services_details = mysql_fetch_row(res_services_details);

            item["id"]=row_services_details[0];
            item["service"]=row_services_details[1];
            //item["image"]=row_services_details[10];
            item["date"]=row_services_details[7];
            item["rating"]=row_service[4];
            item["content"]=row_service[5];
            item["images"]=row_services_details[10];
            historyReviews.push_back(item);
           }
        }
        json response = {
            {"success", true},
            {"message", "查询成功"},
            {"pendingServices", pendingServices},
            {"historyReviews", historyReviews}
        };
        // 清理资源
        mysql_close(conn);
        
    res.set_content(response.dump(), "application/json");
    } catch (const std::exception& e) {
        res.status = 500;
        res.set_content(u8"{\"success\": false, \"message\": \"" + std::string(e.what()) + "\"}", "application/json");
    }
});

svr.Get("/coupons", [](const httplib::Request& req, httplib::Response& res) {
    try {
        //res.set_redirect("/userProfile");
        // 获取identify参数
        
        std::string identify = req.get_param_value("phone");
        if (identify.empty()) {
            res.status = 400;
            res.set_content(u8"{\"success\": false, \"message\": \"缺少identify参数\"}", "application/json");
            return;
        }

        // 连接数据库
        MYSQL* conn = connectDB();
        if (!conn) {
            res.status = 500;
            res.set_content(u8"{\"success\": false, \"message\": \"数据库连接失败\"}", "application/json");
            return;
        }
        

        // 构造查询语句
        std::string sql = "SELECT * FROM users WHERE ";
        if (identify.find('@') != std::string::npos) {
            sql += "email = '"+identify+"'";
        } else {
            sql += "phone = '"+identify+"'";
        }

        cout<<sql<<endl;

        if (mysql_ping(conn) != 0) {
            std::cerr << "Connection lost: " << mysql_error(conn) << std::endl;
        }

        json result_user_id = executeSelectQuery(conn, sql);

        if (mysql_query(conn, sql.c_str())) {
            std::cerr << u8"执行失败: " << mysql_error(conn) << std::endl;
            mysql_close(conn);
            return;
        }

         MYSQL_RES* res_user_id = mysql_store_result(conn);
        if (!res_user_id) {
            std::cerr << u8"获取结果集失败: " << mysql_error(conn) << std::endl;
            mysql_close(conn);
            return;
        }

        MYSQL_ROW row_user_id= mysql_fetch_row(res_user_id);

        string user_id=row_user_id[0];

        // 构造查询语句
        std::string sql_coupon = "SELECT * FROM coupons WHERE user_id = '"+user_id+"'";
        cout<<sql_coupon<<endl;
        if (mysql_query(conn, sql_coupon.c_str())) {
            std::cerr << u8"执行失败: " << mysql_error(conn) << std::endl;
            mysql_close(conn);
            return;
        }
        MYSQL_RES* res_coupon = mysql_store_result(conn);
        if (!res_coupon) {
            std::cerr << u8"获取结果集失败: " << mysql_error(conn) << std::endl;
            mysql_close(conn);
            return;
        }

        MYSQL_ROW row_coupon;
        json coupons;
        while(row_coupon = mysql_fetch_row(res_coupon))
        {
            json item;
            string fdate=row_coupon[6];
            string tdate=row_coupon[7];
            item["id"]=row_coupon[0];
            item["title"]=row_coupon[1];
            item["description"]=row_coupon[2]? row_coupon[2] : "";
            item["type"]=row_coupon[3];
            item["value"]=row_coupon[4];
            item["status"]=row_coupon[5];
            item["validDate"]=fdate+" to "+tdate;
            item["minAmount"]=row_coupon[11];
            coupons.push_back(item);
        }
       
       // res.set_content(response.dump(), "application/json");

    
        // 清理资源
        mysql_close(conn);
        json response = {
                    {"success", true}
                };
                response["coupons"] = coupons;
    res.set_content(response.dump(), "application/json");
    } catch (const std::exception& e) {
        res.status = 500;
        res.set_content(u8"{\"success\": false, \"message\": \"" + std::string(e.what()) + "\"}", "application/json");
    }
});

    // 在main函数中添加新路由（在/dashboard路由之后添加）
// 在文档1的main函数中添加路由处理
svr.Get("/svc_search", [](const httplib::Request& req, httplib::Response& res) {
    try {
        // 参数获取与校验
        std::string service_type = req.get_param_value("service_type");
        std::string price_range = req.get_param_value("price_range");
        std::string start_date = req.get_param_value("start_date");
        std::string end_date = req.get_param_value("end_date");

        // 参数有效性校验
        //if (price_range.empty() && service_type.empty() && 
        //    start_date.empty() && end_date.empty()) {
        //    throw std::runtime_error("至少需要选择一个搜索条件");
        //}

        // 构建查询参数
        std::vector<std::pair<std::string, MYSQL_BIND>> params;
        std::string where_clause = "WHERE 1=1";

        // 服务类型过滤
        if (!service_type.empty()) {
            where_clause += " AND service_type = '"+service_type+"'";
            //MYSQL_BIND bind;1
            //bind.buffer_type = MYSQL_TYPE_STRING;
            //bind.buffer = (void*)service_type.c_str();
            //bind.buffer_length = service_type.size();
            //params.emplace_back("service_type", bind);
        }

        // 价格区间过滤
        if (!price_range.empty()) {
            double min = 0, max = 0;
            if (price_range == "0-100") {
                min = 0; max = 100;
            } else if (price_range == "100-300") {
                min = 100; max = 300;
            } else if (price_range == "500-1000") {
                min = 500; max = 1000;
            } else if (price_range == "1000+") {
                min = 1000;
            } else {
                throw std::runtime_error("无效价格区间");
            }

            where_clause += " AND price BETWEEN "+to_string(min)+" AND "+to_string(max);
            //MYSQL_BIND min_bind, max_bind;
            //min_bind.buffer_type = MYSQL_TYPE_DOUBLE;
            //min_bind.buffer = &min;
            //max_bind.buffer_type = MYSQL_TYPE_DOUBLE;
            //max_bind.buffer = &max;
            //params.emplace_back("min_price", min_bind);
            //params.emplace_back("max_price", max_bind);
        }

        // 时间范围过滤
        if (!start_date.empty() || !end_date.empty()) {
            MYSQL_TIME start_tm = {}, end_tm = {};
            //if (!start_date.empty()) {
            //    std::tm tm = {};
            //    std::istringstream ss(start_date);
            //    ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M");
            //    if (ss.fail()) throw std::runtime_error("无效开始时间");
            //    start_tm.year = tm.tm_year + 1900;
            //    start_tm.month = tm.tm_mon + 1;
            //    start_tm.day = tm.tm_mday;
            //    start_tm.hour = tm.tm_hour;
            //    start_tm.minute = tm.tm_min;
            //    start_tm.second = tm.tm_sec;
            //}
            //if (!end_date.empty()) {
            //    std::tm tm = {};
            //    std::istringstream ss(end_date);
            //    ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M");
            //    if (ss.fail()) throw std::runtime_error("无效结束时间");
            //    end_tm.year = tm.tm_year + 1900;
            //    end_tm.month = tm.tm_mon + 1;
            //    end_tm.day = tm.tm_mday;
            //    end_tm.hour = tm.tm_hour;
            //    end_tm.minute = tm.tm_min;
            //    end_tm.second = tm.tm_sec;
            //}

            for(int i=0;i<start_date.size();i++)
            {
                if(start_date[i]=='T')
                {
                    start_date[i]=' ';
                }
            }

            for(int i=0;i<end_date.size();i++)
            {
                if(end_date[i]=='T')
                {
                    end_date[i]=' ';
                }
            }

            where_clause += " AND available_time BETWEEN '"+start_date+":00"+"' AND '"+end_date+":00'";
            params.emplace_back("start_time", 
                create_datetime_bind(start_tm));
            params.emplace_back("end_time", 
                create_datetime_bind(end_tm));
        }

        // 执行查询
        MYSQL* conn = connectDB();
        if (!conn) throw std::runtime_error("数据库连接失败");
        

        MYSQL_STMT* stmt = mysql_stmt_init(conn);
        if (!stmt) throw std::runtime_error(mysql_error(conn));

        std::string query = "SELECT * FROM services " + where_clause;
        cout<<query;
        if (mysql_stmt_prepare(stmt, query.c_str(), query.length())) {
            throw std::runtime_error(mysql_stmt_error(stmt));
        }

        // 绑定参数
        //std::vector<MYSQL_BIND> binds;
        //for (const auto& p : params) {
        //    binds.push_back(p.second);
        //}
        //if (mysql_stmt_bind_param(stmt, binds.data())) {
        //    throw std::runtime_error(mysql_stmt_error(stmt));
        //}

        //if (mysql_stmt_execute(stmt)) {
        //    throw std::runtime_error(mysql_stmt_error(stmt));
        //}

        if (mysql_ping(conn) != 0) {
            std::cerr << "Connection lost: " << mysql_error(conn) << std::endl;

        }

        // 获取结果
        json result = executeSelectQuery(conn, query);

        if (mysql_query(conn, query.c_str())) {
            std::cerr << u8"执行失败: " << mysql_error(conn) << std::endl;
            mysql_close(conn);
            return;
        }

        MYSQL_RES* res1 = mysql_store_result(conn);
        if (!res1) {
            std::cerr << u8"获取结果集失败: " << mysql_error(conn) << std::endl;
            mysql_close(conn);
            return;
        }

        // 清理资源
        mysql_stmt_close(stmt);
        mysql_close(conn);

        MYSQL_ROW row;
        while ((row = mysql_fetch_row(res1))) {
           std::cout << "ID: " << row[0] 
                      << ", 名称: " << row[1] 
                      << ", 价格: " << row[2] << std::endl;
        }

        // 返回结果
        res.set_content(result.dump(), "application/json");

    } catch (const std::exception& e) {
        res.status = 400;
        res.set_content(u8"{\"success\": false, \"message\": \"" +
                        std::string(e.what()) + "\"}", "application/json");
    }
});

svr.Get("/bookingRecords", [](const httplib::Request& req, httplib::Response& res){
    
    std::string phone = req.get_param_value("userPhone");

    MYSQL* conn = connectDB();
    if (!conn) throw std::runtime_error("数据库连接失败");

    std::string query_user_id = "SELECT id FROM users WHERE phone = '"+phone+"'";

    
    cout<<query_user_id<<endl;
    json result_user_id = executeSelectQuery(conn, query_user_id);

    if (mysql_query(conn, query_user_id.c_str())) {
        std::cerr << u8"执行失败: " << mysql_error(conn) << std::endl;
        mysql_close(conn);
        return;
    }

    MYSQL_RES* res_user_id = mysql_store_result(conn);
    if (!res_user_id) {
        std::cerr << u8"获取结果集失败: " << mysql_error(conn) << std::endl;
        mysql_close(conn);
        return;
    }
    MYSQL_ROW row_user_id= mysql_fetch_row(res_user_id);
    string user_id=row_user_id[0];

    std::string query_get = "SELECT * FROM appointment_records WHERE user_id = '"+user_id+"'";
    json result_get = executeSelectQuery(conn, query_get);

    if (mysql_query(conn, query_get.c_str())) {
        std::cerr << u8"执行失败: " << mysql_error(conn) << std::endl;
        mysql_close(conn);
        return;
    }

    MYSQL_RES* res_get = mysql_store_result(conn);
    if (!res_get) {
        std::cerr << u8"获取结果集失败: " << mysql_error(conn) << std::endl;
        mysql_close(conn);
        return;
    }
    json result;
    json servicesArray = json::array();

    int count=0;
    int pending=0;
    int confirmed=0;
    int completed=0;
    int cancelled=0;

    MYSQL_ROW row_get;
     while ((row_get = mysql_fetch_row(res_get)))
     {
        count++;
        string service_id=row_get[1];
        json item;
        std::string query_service = "SELECT * FROM services WHERE id = '"+service_id+"'";
        cout<<query_service<<endl;
        json result_service = executeSelectQuery(conn, query_service);
        if (mysql_query(conn, query_service.c_str())) {
            std::cerr << u8"执行失败: " << mysql_error(conn) << std::endl;
            mysql_close(conn);
            return;
        }
        MYSQL_RES* res_service = mysql_store_result(conn);
        if (!res_service) {
            std::cerr << u8"获取结果集失败: " << mysql_error(conn) << std::endl;
            mysql_close(conn);
            return;
        }
        MYSQL_ROW row_service=mysql_fetch_row(res_service);
        if (strcmp(row_get[3], "pending") == 0)pending++;
        if (strcmp(row_get[3], "confirmed") == 0)confirmed++;
        if (strcmp(row_get[3], "completed") == 0)completed++;
        if (strcmp(row_get[3], "cancelled") == 0)cancelled++;
        item["id"] = row_service[0];
        item["service"] = row_service[1];
        item["category"] = row_service[2];
        item["image"] = row_service[10];
        item["status"] = row_get[3];
        item["time"] = row_service[7];
        item["location"] = row_service[9]?row_service[9]:"";
        item["price"] = row_service[5];
        item["provider"] = row_service[3];
        //// 其他字段可根据需要选择性添加
        
        servicesArray.push_back(item);
     }

    result["records"] = servicesArray;

    json counts;
    counts["all"]=count;
    counts["pending"]=pending;
    counts["confirmed"]=confirmed;
    counts["completed"]=completed;
    counts["cancelled"]=cancelled;

    result["counts"]=counts;

    res.set_content(
        result.dump(),
        "application/json"
    );
});

svr.Post("/bookingCancel", [](const httplib::Request& req, httplib::Response& res){
    json j=json::parse(req.body);
    std::string phone = j.at("identifier").get<std::string>();
    std::string service_id = j.at("recordId").get<std::string>();

    MYSQL* conn = connectDB();
    if (!conn) throw std::runtime_error("数据库连接失败");

    std::string query_user_id = "SELECT id FROM users WHERE phone = '"+phone+"'";

    
    cout<<query_user_id<<endl;
    json result_user_id = executeSelectQuery(conn, query_user_id);

    if (mysql_query(conn, query_user_id.c_str())) {
        std::cerr << u8"执行失败: " << mysql_error(conn) << std::endl;
        mysql_close(conn);
        return;
    }

    MYSQL_RES* res_user_id = mysql_store_result(conn);
    if (!res_user_id) {
        std::cerr << u8"获取结果集失败: " << mysql_error(conn) << std::endl;
        mysql_close(conn);
        return;
    }
    MYSQL_ROW row_user_id= mysql_fetch_row(res_user_id);
    string user_id=row_user_id[0];

    std::string query_get = "UPDATE appointment_records SET status = 'cancelled' WHERE user_id = '"
                            +user_id+"' AND service_id = '"+service_id+"'";

    cout<<query_get<<endl;                        
    json result_get = executeSelectQuery(conn, query_get);

    if (mysql_query(conn, query_get.c_str())) {
        std::cerr << u8"执行失败: " << mysql_error(conn) << std::endl;
        mysql_close(conn);
        return;
    }

    res.set_content(
        "{\"success\":true,\"message\":\"预约取消成功\"}",
        "application/json"
    );
});

svr.Post("/bookingsBatchCancel", [](const httplib::Request& req, httplib::Response& res){
    json j=json::parse(req.body);
    std::string phone = j.at("identifier").get<std::string>();
    std::vector<string> recordIds = j.at("recordIds").get<std::vector<string>>();

    MYSQL* conn = connectDB();
    if (!conn) throw std::runtime_error("数据库连接失败");

    std::string query_user_id = "SELECT id FROM users WHERE phone = '"+phone+"'";

    
    cout<<query_user_id<<endl;
    json result_user_id = executeSelectQuery(conn, query_user_id);

    if (mysql_query(conn, query_user_id.c_str())) {
        std::cerr << u8"执行失败: " << mysql_error(conn) << std::endl;
        mysql_close(conn);
        return;
    }

    MYSQL_RES* res_user_id = mysql_store_result(conn);
    if (!res_user_id) {
        std::cerr << u8"获取结果集失败: " << mysql_error(conn) << std::endl;
        mysql_close(conn);
        return;
    }
    MYSQL_ROW row_user_id= mysql_fetch_row(res_user_id);
    string user_id=row_user_id[0];

    for(int i=0;i<recordIds.size();i++)
    {
        std::string service_id = recordIds[i];
        std::string query_get = "UPDATE appointment_records SET status = 'cancelled' WHERE user_id = '"
                            +user_id+"' AND service_id = '"+service_id+"'";

        cout<<query_get<<endl;                        
        json result_get = executeSelectQuery(conn, query_get);

        if (mysql_query(conn, query_get.c_str())) {
            std::cerr << u8"执行失败: " << mysql_error(conn) << std::endl;
            mysql_close(conn);
            return;
        }
    }

    //std::string query_get = "UPDATE appointment_records SET status = 'cancelled' WHERE user_id = '"
    //                        +user_id+"' AND service_id = '"+service_id+"'";
//
   // cout<<query_get<<endl;                        
    //json result_get = executeSelectQuery(conn, query_get);
//
    //if (mysql_query(conn, query_get.c_str())) {
    //    std::cerr << u8"执行失败: " << mysql_error(conn) << std::endl;
    //    mysql_close(conn);
    //    return;
    //}

    res.set_content(
        "{\"success\":true,\"message\":\"预约取消成功\"}",
        "application/json"
    );
});

svr.Post("/reviewsSubmit", [](const httplib::Request& req, httplib::Response& res){
    auto content_type_it = req.headers.find("Content-Type");
            if (content_type_it == req.headers.end() || 
                content_type_it->second != "application/json") {
                throw std::runtime_error("Invalid Content-Type");
            }
    json j=json::parse(req.body);

    std::string serviceId=j.at("serviceId").get<std::string>();
    std::string identifier=j.at("identifier").get<std::string>();
    int rating=j.at("rating").get<int>();
    std::string content=j.at("content").get<std::string>();

    MYSQL* conn = connectDB();
    if (!conn) throw std::runtime_error("数据库连接失败");


    //查找服务
    std::string query_user = "SELECT id FROM users WHERE phone = '" +identifier + "'";
    cout<<query_user<<endl;
    json result_user=executeSelectQuery(conn,query_user);

    if (mysql_query(conn, query_user.c_str())) {
            std::cerr << u8"执行失败: " << mysql_error(conn) << std::endl;
            mysql_close(conn);
            return;
    }

    MYSQL_RES* res_user = mysql_store_result(conn);
        if (!res_user) {
            std::cerr << u8"获取结果集失败: " << mysql_error(conn) << std::endl;
            mysql_close(conn);
            return;
        }
    MYSQL_ROW row_user= mysql_fetch_row(res_user);
    string id_user=row_user[0];

    std::string query_commnet = "UPDATE appointment_records SET rating=" + to_string(rating)
                 + ", review='" + content + "'"
                 + " WHERE user_id=" + id_user
                 + " AND service_id=" + serviceId;
    cout<<query_commnet<<endl;
    json result_comment=executeSelectQuery(conn,query_user);



    //std::string query_update_rating = "UPDATE services SET rating = " + std::to_string(average_rating) + " WHERE id = '" + serviceId + "'";
   //cout<<query_update_rating<<endl;
    //json result_update_rating=executeSelectQuery(conn,query_update_rating);
    //if (mysql_query(conn, query_update_rating.c_str())) {
    //        std::cerr << u8"执行失败: " << mysql_error(conn) << std::endl;
    //        mysql_close(conn);
    //        return;
    //}



    if (mysql_query(conn, query_commnet.c_str())) {
            std::cerr << u8"执行失败: " << mysql_error(conn) << std::endl;
            mysql_close(conn);
            return;
    }

    
    res.set_content(u8"{\"success\": true, \"message\": \"评价成功\"}", "application/json");
});

    // 添加首页路由
svr.Get("/home", [](const httplib::Request&, httplib::Response& res) {
    std::ifstream in("static/home.html");
    std::string html((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    res.set_content(html, "text/html; charset=utf-8");
});

// 添加服务中心路由
svr.Get("/service", [](const httplib::Request&, httplib::Response& res) {
    std::ifstream in("static/service.html");
    std::string html((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    res.set_content(html, "text/html; charset=utf-8");
});

// 添加记录页面路由
svr.Get("/record", [](const httplib::Request&, httplib::Response& res) {
    std::ifstream in("static/record.html");
    std::string html((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    res.set_content(html, "text/html; charset=utf-8");
});

// 添加时间管理路由
svr.Get("/time", [](const httplib::Request&, httplib::Response& res) {
    std::ifstream in("static/time.html");
    std::string html((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    res.set_content(html, "text/html; charset=utf-8");
});

// 添加用户管理路由
svr.Get("/user", [](const httplib::Request&, httplib::Response& res) {
    std::ifstream in("static/user.html");
    std::string html((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    res.set_content(html, "text/html; charset=utf-8");
});

    

    // 启动服务器（监听 8080 端口）
    std::cout << "serlaunch listening.. 49680..." << endl;
    svr.listen("localhost", 49680);

   
    return 0;
}