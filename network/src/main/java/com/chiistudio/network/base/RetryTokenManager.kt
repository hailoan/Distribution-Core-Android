package com.chiistudio.network.base

import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Deferred
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.async
import kotlinx.coroutines.sync.Mutex
import kotlinx.coroutines.sync.withLock

class RetryTokenManager {

    val mutex: Mutex = Mutex()
    private var executeJob: Deferred<Result<Unit>>? = null
    private val coroutineScope = CoroutineScope(Dispatchers.IO)
    suspend fun handleRefreshToken(executeTask: () -> Result<Unit>): Result<Unit> {
        if (executeJob != null) {
            return executeJob!!.await()
        }
        mutex.withLock {
            executeJob?.let { return it.await() }
            val newJob = coroutineScope.async {
                executeTask.invoke()
            }
            executeJob = newJob
            try {
                val result = newJob.await()
                return result
            } finally {
                executeJob = null
            }
        }
    }

}