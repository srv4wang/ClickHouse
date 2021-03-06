#include <Access/Authentication.h>
#include <Common/Exception.h>
#include <Poco/SHA1Engine.h>


namespace DB
{
namespace ErrorCodes
{
    extern const int REQUIRED_PASSWORD;
    extern const int WRONG_PASSWORD;
}


Authentication::Digest Authentication::getPasswordDoubleSHA1() const
{
    switch (type)
    {
        case NO_PASSWORD:
        {
            Poco::SHA1Engine engine;
            return engine.digest();
        }

        case PLAINTEXT_PASSWORD:
        {
            Poco::SHA1Engine engine;
            engine.update(getPassword());
            const Digest & first_sha1 = engine.digest();
            engine.update(first_sha1.data(), first_sha1.size());
            return engine.digest();
        }

        case SHA256_PASSWORD:
            throw Exception("Cannot get password double SHA1 for user with 'SHA256_PASSWORD' authentication.", ErrorCodes::BAD_ARGUMENTS);

        case DOUBLE_SHA1_PASSWORD:
            return password_hash;
    }
    throw Exception("Unknown authentication type: " + std::to_string(static_cast<int>(type)), ErrorCodes::LOGICAL_ERROR);
}


bool Authentication::isCorrectPassword(const String & password_) const
{
    switch (type)
    {
        case NO_PASSWORD:
            return true;

        case PLAINTEXT_PASSWORD:
        {
            if (password_ == std::string_view{reinterpret_cast<const char *>(password_hash.data()), password_hash.size()})
                return true;

            // For compatibility with MySQL clients which support only native authentication plugin, SHA1 can be passed instead of password.
            auto password_sha1 = encodeSHA1(password_hash);
            return password_ == std::string_view{reinterpret_cast<const char *>(password_sha1.data()), password_sha1.size()};
        }

        case SHA256_PASSWORD:
            return encodeSHA256(password_) == password_hash;

        case DOUBLE_SHA1_PASSWORD:
        {
            auto first_sha1 = encodeSHA1(password_);

            /// If it was MySQL compatibility server, then first_sha1 already contains double SHA1.
            if (first_sha1 == password_hash)
                return true;

            return encodeSHA1(first_sha1) == password_hash;
        }
    }
    throw Exception("Unknown authentication type: " + std::to_string(static_cast<int>(type)), ErrorCodes::LOGICAL_ERROR);
}


void Authentication::checkPassword(const String & password_, const String & user_name) const
{
    if (isCorrectPassword(password_))
        return;
    auto info_about_user_name = [&user_name]() { return user_name.empty() ? String() : " for user " + user_name; };
    if (password_.empty() && (type != NO_PASSWORD))
        throw Exception("Password required" + info_about_user_name(), ErrorCodes::REQUIRED_PASSWORD);
    throw Exception("Wrong password" + info_about_user_name(), ErrorCodes::WRONG_PASSWORD);
}

}
