#pragma once

#include "goodreader/application/ports.hpp"

namespace goodreader::application {

/** Local-only authentication implementation used until account features are introduced. */
class LocalAuthService final : public AuthService {
  public:
    /** Returns the stable local-only user. */
    AppResult<LocalUser> current_user() override;
};

/** Default MVP entitlement implementation, which unlocks all features locally. */
class UnlockedEntitlementService final : public EntitlementService {
  public:
    /** Returns enabled for every MVP feature. */
    AppResult<bool> is_enabled(Feature feature) override;
};

} // namespace goodreader::application
