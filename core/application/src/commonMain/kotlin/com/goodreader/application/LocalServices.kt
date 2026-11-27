package com.goodreader.application

/** Local-only authentication implementation used until account features are introduced. */
class LocalAuthService : AuthService {
    override suspend fun currentUser(): AppResult<LocalUser> = AppResult.Success(LocalUser(id = "local-user", isLocalOnly = true))
}

/** Default MVP entitlement implementation, which unlocks all features locally. */
class UnlockedEntitlementService : EntitlementService {
    override suspend fun isEnabled(feature: Feature): AppResult<Boolean> = AppResult.Success(true)
}
