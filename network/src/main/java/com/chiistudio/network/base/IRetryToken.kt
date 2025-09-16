package com.chiistudio.network.base

import io.ktor.client.statement.HttpResponse

interface IRetryToken {

    suspend fun shouldRetryToken(): Boolean

    suspend fun needRetryToken(response: HttpResponse): Boolean

    suspend fun isExpireToken(): Boolean

    suspend fun refreshTokensApi(): Unit

}