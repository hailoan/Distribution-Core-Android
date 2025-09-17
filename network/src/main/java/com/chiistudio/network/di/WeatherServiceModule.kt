package com.chiistudio.network.di

import com.chiistudio.network.InitNetwork
import com.chiistudio.network.base.IClient
import com.chiistudio.network.service.weather.BearWeatherAuth
import com.chiistudio.network.service.weather.BearWeatherClient
import com.chiistudio.network.service.weather.BearWeatherHeader
import com.chiistudio.network.service.weather.BearWeatherService
import dagger.Module
import dagger.Provides
import dagger.hilt.InstallIn
import dagger.hilt.components.SingletonComponent
import javax.inject.Qualifier
import javax.inject.Singleton

@Module
@InstallIn(SingletonComponent::class)
class WeatherServiceModule {
    @Provides
    @Singleton
    @WeatherClient
    fun provideWeatherClient(): IClient = BearWeatherClient()

    @Provides
    @Singleton
    @WeatherAuth
    fun provideWeatherAuth(
        @WeatherClient client: IClient
    ): BearWeatherAuth = BearWeatherAuth(client) { InitNetwork.weatherRetryToken }

    @Provides
    @Singleton
    @WeatherHeader
    fun provideWeatherHeader(
        @WeatherClient client: IClient
    ): BearWeatherHeader = BearWeatherHeader(client)

    @Provides
    @Singleton
    fun provideWeatherService(
        @WeatherClient client: BearWeatherClient,
        @WeatherAuth auth: BearWeatherAuth,
        @WeatherHeader header: BearWeatherHeader
    ): BearWeatherService = BearWeatherService(client, auth, header)

}

@Qualifier
@Retention(AnnotationRetention.BINARY)
annotation class WeatherAuth

@Qualifier
@Retention(AnnotationRetention.BINARY)
annotation class WeatherClient

@Qualifier
@Retention(AnnotationRetention.BINARY)
annotation class WeatherHeader
