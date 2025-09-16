package com.chiistudio.network.service.weather

import com.chiistudio.network.base.IClient
import com.chiistudio.network.di.WeatherClient
import javax.inject.Inject

class BearWeatherHeader @Inject constructor(
    @WeatherClient val client: IClient
) : IClient by client {

    init {
        client.defaultHeader.apply {
            //TODO
        }
    }
}