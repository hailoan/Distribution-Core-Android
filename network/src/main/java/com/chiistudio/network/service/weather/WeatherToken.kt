package com.chiistudio.network.service.weather

import com.chiistudio.network.base.IRetryToken
import com.chiistudio.network.base.ITokenManager
import io.ktor.client.statement.HttpResponse

class WeatherRetryToken: IRetryToken {
    override suspend fun shouldRetryToken(): Boolean {
        TODO("Not yet implemented")
    }

    override suspend fun needRetryToken(response: HttpResponse): Boolean {
        TODO("Not yet implemented")
    }

    override suspend fun isExpireToken(): Boolean {
        TODO("Not yet implemented")
    }

    override suspend fun refreshTokensApi() {
        TODO("Not yet implemented")
    }

}

class WeatherTokenManager : ITokenManager {
    override suspend fun getAccessToken(): Pair<String, String> {
        return Pair("", "")
    }
}