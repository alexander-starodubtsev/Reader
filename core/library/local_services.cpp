#include "goodreader/application/local_services.hpp"

#include <utility>

namespace goodreader::application {

AppResult<LocalUser> LocalAuthService::current_user() {
    return AppResult<LocalUser>::success(LocalUser{"local-user", true});
}

AppResult<bool> UnlockedEntitlementService::is_enabled(Feature feature) {
    static_cast<void>(feature);
    return AppResult<bool>::success(true);
}

} // namespace goodreader::application
