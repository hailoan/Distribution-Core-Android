package com.chiistudio.core.basemvvm

import kotlinx.coroutines.currentCoroutineContext
import kotlinx.coroutines.sync.Mutex
import kotlinx.coroutines.sync.withLock
import kotlin.coroutines.CoroutineContext

class ReentrantMutex {
    private val mutex = Mutex()
    private var owner: CoroutineContext? = null
    private var holdCount = 0

    suspend fun <T> withReentrantLock(block: suspend () -> T): T {
        val context = currentCoroutineContext()
        return if (owner === context) {
            holdCount++
            try {
                block()
            } finally {
                holdCount--
                if (holdCount == 0) owner = null
            }
        } else {
            mutex.withLock {
                owner = context
                holdCount = 1
                try {
                    block()
                } finally {
                    holdCount = 0
                    owner = null
                }
            }
        }
    }
}