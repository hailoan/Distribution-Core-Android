package com.chiistudio.core

import com.chiistudio.core.basemvvm.ReentrantMutex
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.Job
import kotlinx.coroutines.async
import kotlinx.coroutines.cancel
import kotlinx.coroutines.delay
import kotlinx.coroutines.joinAll
import kotlinx.coroutines.launch
import kotlinx.coroutines.runBlocking
import kotlinx.coroutines.test.UnconfinedTestDispatcher
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Test

@OptIn(ExperimentalCoroutinesApi::class)
class ReentrantMutexUnitTest {

    private val testDispatcher = UnconfinedTestDispatcher()
    private val testScope = CoroutineScope(testDispatcher + Job())

    private lateinit var mutex: ReentrantMutex

    @Before
    fun setup() {
        mutex = ReentrantMutex()
    }

    @After
    fun tearDown() {
        testScope.cancel()
    }

    @Test
    fun `basic lock works`() = runBlocking {
        var value = 0
        mutex.withReentrantLock {
            value++
        }
        assertEquals(1, value)
    }

    @Test
    fun `reentrant lock works in same coroutine`() = runBlocking {
        var value = 0
        mutex.withReentrantLock {
            value++
            mutex.withReentrantLock {
                value += 2
            }
        }
        assertEquals(3, value) // 1 + 2
    }

    @Test
    fun `mutual exclusion prevents concurrent access`() = runBlocking {
        val results = mutableListOf<Int>()

        val job1 = async {
            mutex.withReentrantLock {
                results.add(1)
                delay(200) // hold lock
                results.add(2)
            }
        }

        val job2 = async {
            delay(50) // ensure job1 acquires first
            mutex.withReentrantLock {
                results.add(3)
            }
        }

        joinAll(job1, job2)

        // Ensure job2 (3) happens only after job1 released lock (2)
        assertEquals(listOf(1, 2, 3), results)
    }

    @Test
    fun `lock released after block finishes`() = runBlocking {
        var value = 0

        // First coroutine acquires lock
        launch {
            mutex.withReentrantLock {
                value = 1
                delay(100)
            }
        }

        // Second coroutine waits then acquires after release
        launch {
            delay(150)
            mutex.withReentrantLock {
                value = 2
            }
        }.join()

        assertEquals(2, value)
    }

}