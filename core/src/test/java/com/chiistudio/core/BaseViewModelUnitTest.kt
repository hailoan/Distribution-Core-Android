package com.chiistudio.core

import androidx.lifecycle.viewModelScope
import com.chiistudio.core.basemvvm.BaseViewModel
import kotlinx.coroutines.Job
import kotlinx.coroutines.async
import kotlinx.coroutines.delay
import kotlinx.coroutines.joinAll
import kotlinx.coroutines.launch
import kotlinx.coroutines.runBlocking
import kotlinx.coroutines.test.UnconfinedTestDispatcher
import kotlinx.coroutines.test.advanceTimeBy
import kotlinx.coroutines.test.runTest
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Rule
import org.junit.Test

class BaseViewModelUnitTest {
    @get:Rule
    val mainDispatcherRule = MainDispatcherRule(UnconfinedTestDispatcher())
    private lateinit var viewModel: DemoViewModel

    @Before
    fun setup() {
        viewModel = DemoViewModel()
    }

    @Test
    fun `when send 2 action Search should be result == last_action`() = runTest {
        val job1 = async {
            viewModel.sendAction(DemoViewModel.Action.Search("first", 200))
        }
        val job2 = async {
            viewModel.sendAction(DemoViewModel.Action.Search("second", 50))
        }
        joinAll(job1, job2)
        advanceTimeBy(300)

        assertEquals(viewModel.state.value.search, "second")
    }

    @Test
    fun `when send 2 action Increase_2000 Increase_1000 should be state_count=2`() = runTest {
        val job1 = async {
            viewModel.sendAction(DemoViewModel.Action.Increase(200))
        }
        val job2 = async {
            viewModel.sendAction(DemoViewModel.Action.Increase(50))
        }
        joinAll(job1, job2)
        advanceTimeBy(300)

        assertEquals(viewModel.state.value.count, 2)
    }

    @Test
    fun `when send 66 actions should be not miss action`() = runTest {

    }
}

class DemoViewModel :
    BaseViewModel<DemoViewModel.State, DemoViewModel.Action, DemoViewModel.Effect>() {
    override var initState: State = State()
    private var jobSearch: Job ?= null

    override fun handleAction(action: Action, state: State, emitState: ((State) -> State) -> Unit) {
        when (action) {
            is Action.Increase -> {
                viewModelScope.launch {
                    delay(action.timeTask)
                    emitState {
                        it.copy(count = it.count + 1)
                    }
                }
            }

            is Action.Decrease -> {
                viewModelScope.launch {
                    delay(action.timeTask)
                    emitState {
                        it.copy(count = it.count - 1)
                    }
                }
            }

            is Action.Search -> {
                jobSearch?.cancel()
                jobSearch = viewModelScope.launch {
                    delay(action.timeTask)
                    emitState {
                        it.copy(search = action.text)
                    }
                }
            }
        }
    }

    sealed class Action : BaseViewModel.VMAction {
        data class Increase(val timeTask: Long) : Action()
        data class Decrease(val timeTask: Long) : Action()
        data class Search(val text: String, val timeTask: Long) : Action()
    }

    sealed interface Effect : BaseViewModel.VMEffect {

    }

    data class State(
        val count: Int = 0,
        val search: String? = null
    ) : BaseViewModel.VMState
}