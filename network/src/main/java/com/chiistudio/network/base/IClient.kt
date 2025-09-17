package com.chiistudio.network.base

import io.ktor.client.HttpClient
import io.ktor.client.statement.HttpResponse
import io.ktor.http.HttpMethod
import java.io.File
import java.util.HashMap

interface IClient {
    val httpClient: HttpClient

    val defaultHeader: HashMap<String, String>

    suspend fun doUpload(
        path: String,
        file: File,
        fieldName: String = "file"
    ): HttpResponse

    // reified version
    suspend fun doRequest(
        method: HttpMethod,
        path: String,
        query: Map<String, String> = emptyMap(),
        body: Any? = null
    ): HttpResponse
}