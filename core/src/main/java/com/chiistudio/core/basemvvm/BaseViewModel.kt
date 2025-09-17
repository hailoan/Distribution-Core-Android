package com.chiistudio.core.basemvvm

import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import kotlinx.coroutines.channels.BufferOverflow
import kotlinx.coroutines.channels.Channel
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.MutableSharedFlow
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.SharedFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asSharedFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.launchIn
import kotlinx.coroutines.flow.map
import kotlinx.coroutines.flow.onEach
import kotlinx.coroutines.flow.receiveAsFlow
import kotlinx.coroutines.launch

abstract class BaseViewModel<S : BaseViewModel.VMState, A : BaseViewModel.VMAction, M : BaseViewModel.VMMutation, E : BaseViewModel.VMEffect> :
    ViewModel() {
    interface VMState
    interface VMAction
    interface VMMutation
    interface VMEffect

    abstract var initState: S
    private val _action: MutableSharedFlow<A> = MutableSharedFlow(
        replay = 0,     // how many past values to replay to new collectors
        extraBufferCapacity = 64,   // how many additional values to buffer
        onBufferOverflow = BufferOverflow.SUSPEND   // what to do if buffer is full
    )
    val action: SharedFlow<A> = _action
    private val _state: MutableStateFlow<S> by lazy { MutableStateFlow(initState) }
    val state: StateFlow<S> by lazy { _state.asStateFlow() }
    private val _mutation by lazy { Channel<M>() }
    val mutation: Flow<M> = _mutation.receiveAsFlow()

    private val _effect: MutableSharedFlow<E> = MutableSharedFlow(
        0, 1, BufferOverflow.DROP_OLDEST
    )
    val effect = _effect.asSharedFlow()


    abstract fun handleAction(action: A, state: S)

    abstract suspend fun handleMutation(mutation: M, state: S): S

    init {
        action.onEach {
            handleAction(it, state.value)
        }.launchIn(viewModelScope)
        mutation.map {
            handleMutation(mutation = it, state = state.value)
        }.onEach {
            setState(it)
        }.launchIn(viewModelScope)
    }

    protected fun setState(state: S) {
        viewModelScope.launch {
            _state.emit(state)
        }
    }

    protected fun sendEffect(effect: E) {
        viewModelScope.launch {
            _effect.emit(effect)
        }
    }

    fun sendAction(action: A) {
        viewModelScope.launch {
            _action.emit(action)
        }
    }

    fun sendMutation(mutation: M) {
        viewModelScope.launch {
            _mutation.send(mutation)
        }
    }
}
