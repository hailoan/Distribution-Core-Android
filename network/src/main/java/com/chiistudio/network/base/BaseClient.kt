package com.chiistudio.network.base

import io.ktor.client.HttpClient
import io.ktor.client.engine.okhttp.OkHttp
import io.ktor.client.plugins.HttpSend
import io.ktor.client.plugins.auth.Auth
import io.ktor.client.plugins.auth.providers.BearerTokens
import io.ktor.client.plugins.auth.providers.bearer
import io.ktor.client.plugins.logging.DEFAULT
import io.ktor.client.plugins.logging.LogLevel
import io.ktor.client.plugins.logging.Logger
import io.ktor.client.plugins.logging.Logging
import io.ktor.client.request.forms.formData
import io.ktor.client.request.forms.submitFormWithBinaryData
import io.ktor.client.request.request
import io.ktor.client.request.setBody
import io.ktor.client.statement.HttpResponse
import io.ktor.http.ContentType
import io.ktor.http.Headers
import io.ktor.http.HttpHeaders
import io.ktor.http.HttpMethod
import io.ktor.http.contentType
import java.io.File
import java.util.HashMap

open class BaseClient(private val onTokenManager: () -> ITokenManager?) : IClient {
    private val tokenManager get() = onTokenManager.invoke()

    override val defaultHeader: HashMap<String, String> = hashMapOf()

    override val httpClient: HttpClient
        get() = HttpClient(OkHttp) {
            install(Logging) {
                level = LogLevel.ALL
                logger = Logger.DEFAULT
            }
            install(HttpSend)

            install(Auth) {
                bearer {
                    loadTokens {
                        tokenManager?.getAccessToken()?.let {
                            BearerTokens(accessToken = it.first, refreshToken = it.second)
                        }
                    }
                }
            }
        }

    override suspend fun doRequest(
        method: HttpMethod,
        path: String,
        query: Map<String, String>,
        body: Any?
    ): HttpResponse =
        httpClient
            .request(path) {
                this.method = method
                url {
                    query.forEach { (k, v) -> parameters.append(k, v) }
                }
                if (body != null) {
                    contentType(ContentType.Application.Json)
                    setBody(body)
                }
            }

    override suspend fun doUpload(path: String, file: File, fieldName: String): HttpResponse =
        httpClient
            .submitFormWithBinaryData(
                url = path,
                formData = formData {
                    append(
                        key = fieldName,
                        value = file.readBytes(),
                        headers = Headers.build {
                            append(HttpHeaders.ContentType, "application/octet-stream")
                            append(HttpHeaders.ContentDisposition, "filename=${file.name}")
                        }
                    )
                }
            )
}