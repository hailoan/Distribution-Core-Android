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
/**
 * Base class for implementing MVVM with a unidirectional data flow inspired by Redux.
 *
 * This ViewModel enforces predictable state management by modeling around four main concepts:
 *
 * - **State [S]**: Immutable snapshot of the UI. The single source of truth, exposed via [state].
 * - **Action [A]**: Events or intents coming from the UI (e.g., button click). Sent via [sendAction].
 * - **Mutation [M]**: Atomic transformations that describe how state should change. Sent via [sendMutation].
 * - **Effect [E]**: One-time, non-persistent events (e.g., navigation, toast). Sent via [sendEffect].
 * ---
 * ## Data Flow
 * ```
 * UI (View)
 *   │
 *   │ sendAction(A)
 *   ▼
 * ViewModel
 *   │ handleAction(A, state)
 *   │
 *   ├─────> sendMutation(M) ────┐
 *   │                           ▼
 *   │                     Channel Queue
 *   │                           │  (sequential)
 *   │                           ▼
 *   │                   handleMutation(M, state)
 *   │                           │
 *   │                           ▼
 *   │                      setState(newState)
 *   │                           │
 *   ▼                           ▼
 * UI observes effect (E)       UI observes state
 * ```
 * ---
 * ## Mutation: Sequential State Updates
 *
 * Mutations [M] are queued in a [Channel].
 * This ensures:
 * - **Sequentiality**: Only one mutation is processed at a time (FIFO order).
 * - **Consistency**: No two mutations can update state simultaneously.
 * - **Predictability**: Each state is the deterministic result of applying mutations in order.
 * ```
 * sendMutation(M1) ─┐
 * sendMutation(M2) ─┼──> [Channel] ──> handleMutation(M, state) → new State
 * sendMutation(M3) ─┘
 * ```
 * Example:
 * ```
 * Action: "LoadUser"
 *   -> sendMutation(Mutation.Loading)
 *   -> sendMutation(Mutation.UserLoaded(user))
 * Mutation:
 *   Mutation.Loading → state.copy(isLoading = true)
 *   Mutation.UserLoaded → state.copy(user = mutation.user)
 * ```
 *
 * ---
 *
 * ## Benefits
 * - **Predictable**: State evolves only through mutations.
 * - **Sequential**: Channel guarantees ordered and isolated state updates.
 * - **Unidirectional**: Flow is always `Action → Mutation → State`.
 * - **Testable**: Actions, mutations, and states can be tested in isolation.
 *
 * @param S Type representing UI state
 * @param A Type representing user/system actions
 * @param M Type representing state mutations
 * @param E Type representing one-time side effects
 */
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
