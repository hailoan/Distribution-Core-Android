package com.chiistudio.network.service.weather

import com.chiistudio.network.InitNetwork
import com.chiistudio.network.base.BaseClient
import javax.inject.Inject

class BearWeatherClient @Inject constructor() : BaseClient(
    onTokenManager = {
        InitNetwork.weatherTokenManager
    }
)