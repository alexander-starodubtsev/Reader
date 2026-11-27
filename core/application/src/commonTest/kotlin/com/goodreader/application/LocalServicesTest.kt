package com.goodreader.application

import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertTrue

class LocalServicesTest {
    @Test
    fun defaultsToLocalIdentityAndUnlocksEveryMvpFeature() =
        runTest {
            assertEquals(
                AppResult.Success(LocalUser("local-user", true)),
                GetCurrentUserUseCase(LocalAuthService())(),
            )
            Feature.entries.forEach { feature ->
                assertEquals(AppResult.Success(true), CheckFeatureUseCase(UnlockedEntitlementService())(feature))
            }
            assertTrue((LocalAuthService().currentUser() as AppResult.Success).value.isLocalOnly)
        }
}
