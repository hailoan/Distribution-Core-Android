package com.chiistudio.network

import com.chiistudio.network.base.IRetryToken
import com.chiistudio.network.base.ITokenManager

object InitNetwork {
    /**
     * weather
     */
    var weatherTokenManager: ITokenManager ?= null
    var weatherRetryToken: IRetryToken ?= null
}