package com.chiistudio.library

import android.os.Bundle
import android.util.Log
import androidx.activity.enableEdgeToEdge
import androidx.appcompat.app.AppCompatActivity
import androidx.core.view.ViewCompat
import androidx.core.view.WindowInsetsCompat
import androidx.lifecycle.lifecycleScope
import com.chiistudio.network.base.RetryTokenManager
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch

class MainActivity2 : AppCompatActivity() {
    val retryToken = RetryTokenManager()
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        enableEdgeToEdge()
        setContentView(R.layout.activity_main)
        ViewCompat.setOnApplyWindowInsetsListener(findViewById(R.id.main)) { v, insets ->
            val systemBars = insets.getInsets(WindowInsetsCompat.Type.systemBars())
            v.setPadding(systemBars.left, systemBars.top, systemBars.right, systemBars.bottom)
            insets
        }

        lifecycleScope.launch {
            val result = retryToken.handleRefreshToken { handleRefreshToke("1", 400) }
            Log.e("LOAN", "scope1 = $result")

        }

        lifecycleScope.launch {
            val result = retryToken.handleRefreshToken { handleRefreshToke("2", 100) }
            Log.e("LOAN", "scope2 = $result")
        }

        lifecycleScope.launch {
            delay(1000)
            val result = retryToken.handleRefreshToken { handleRefreshToke("3", 200) }
            Log.e("LOAN", "scope3 = $result")
        }
    }

    fun handleRefreshToke(data: String, time: Long): String {
        Thread.sleep(time)
        return data
    }
}