package com.chiistudio.network

import com.chiistudio.network.base.RetryTokenManager
import kotlinx.coroutines.delay
import kotlinx.coroutines.test.runTest
import org.junit.Test

class RetryTokenUnitTest {
    val retryToken = RetryTokenManager()

    @Test
    fun `test retry token`() = runTest {
        retryToken.handleRefreshToken {
            handleRefreshToke("1", 400)
        }

        retryToken.handleRefreshToken {
            handleRefreshToke("2", 100)
        }

        retryToken.handleRefreshToken {
            handleRefreshToke("3", 200)
        }
    }

    suspend fun handleRefreshToke(data: String, time: Long): String {
        delay(time)
        return data
    }
}