package com.chiistudio.network.base

interface ITokenManager {
    suspend fun getAccessToken(): Pair<String, String>
}