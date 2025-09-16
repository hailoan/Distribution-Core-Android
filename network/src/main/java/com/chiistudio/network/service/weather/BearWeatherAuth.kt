package com.chiistudio.network.service.weather

import com.chiistudio.network.base.IClient
import com.chiistudio.network.base.IRetryToken
import com.chiistudio.network.di.WeatherClient
import io.ktor.client.plugins.HttpSend
import io.ktor.client.plugins.plugin
import io.ktor.client.request.HttpRequestBuilder
import javax.inject.Inject

class BearWeatherAuth @Inject constructor(
    @WeatherClient val client: IClient,
    private val onRetryToken: () -> IRetryToken?
) : IClient by client {

    private val retryToken get() = onRetryToken.invoke()

    init {
        client.httpClient.plugin(HttpSend).intercept { request ->
            if (retryToken?.shouldRetryToken() == true) {
                retryToken?.refreshTokensApi()
                request.updateToken()
            }

            val response = execute(request)
            if (retryToken?.needRetryToken(response.response) == true) {
                retryToken?.refreshTokensApi()
                request.updateToken()
                return@intercept execute(request)
            }
            response
        }
    }

    private fun HttpRequestBuilder.updateToken() {
        headers.remove("Authorization")
        headers.append("Authorization", "Bearer ")
    }
}