package com.chiistudio.library

import android.os.Bundle
import android.util.Log
import androidx.activity.enableEdgeToEdge
import androidx.appcompat.app.AppCompatActivity
import androidx.core.view.ViewCompat
import androidx.core.view.WindowInsetsCompat
import androidx.lifecycle.lifecycleScope
import kotlinx.coroutines.channels.BufferOverflow
import kotlinx.coroutines.delay
import kotlinx.coroutines.flow.MutableSharedFlow
import kotlinx.coroutines.flow.launchIn
import kotlinx.coroutines.flow.onEach
import kotlinx.coroutines.launch

class MainActivity : AppCompatActivity() {
    val flow = MutableSharedFlow<Int>(
        replay = 0,
        extraBufferCapacity = 2,
        onBufferOverflow = BufferOverflow.DROP_OLDEST
    )

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        var i = 0
        flow.onEach {
            Log.e("LOAN", "collecting $it")
            delay(2000)
            Log.e("LOAN", "collected $it")
        }.launchIn(lifecycleScope)
        for (i in 0 until 6) {
            lifecycleScope.launch {
                Log.e("LOAN", "emitting $i")
                flow.emit(i)

                Log.e("LOAN", "emit done $i")
            }
        }

    }
}

