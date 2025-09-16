package com.chiistudio.network.service.weather

import com.chiistudio.network.base.IClient
import javax.inject.Inject

class BearWeatherService @Inject constructor(
    val client: BearWeatherClient,
    val auth: BearWeatherAuth,
    val header: BearWeatherHeader
): IClient by client