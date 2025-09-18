package com.chiistudio.core

import androidx.lifecycle.viewModelScope
import com.chiistudio.core.BaseViewModelUnitTest.DemoViewModel.Mutation.*
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
            viewModel.sendAction(DemoViewModel.Action.Search("first", 10))
        }
        val job2 = async {
            viewModel.sendAction(DemoViewModel.Action.Search("second", 5))
        }
        joinAll(job1, job2)
        advanceTimeBy(15)
        assertEquals(viewModel.state.value.search, "second")
    }

    @Test
    fun `when send 2 action Increase_1 Increase_2 Decrease_4 should be state_count=-1`() = runTest {
        val job1 = async {
            viewModel.sendAction(DemoViewModel.Action.Increase(1))
        }
        val job2 = async {
            viewModel.sendAction(DemoViewModel.Action.Increase(2))
        }
        val job3 = async {
            viewModel.sendAction(DemoViewModel.Action.Decrease(4))
        }
        joinAll(job1, job2, job3)
        assertEquals(viewModel.state.value.number, -1)
    }

    @Test
    fun `when send StringConcatenation 1,4mms 2,1mms 3,5mms should be 2_1_3`() = runTest {
        val job1 = async {
            viewModel.sendAction(DemoViewModel.Action.StringConcatenation(text = "1", timeTask = 4))
        }
        val job2 = async {
            viewModel.sendAction(DemoViewModel.Action.StringConcatenation(text = "2", timeTask = 1))
        }
        val job3 = async {
            viewModel.sendAction(DemoViewModel.Action.StringConcatenation(text = "3", timeTask = 5))
        }
        joinAll(job1, job2, job3)
        advanceTimeBy(10)
        assertEquals(viewModel.state.value.concatenation, "2_1_3")
    }

    internal class DemoViewModel :
        BaseViewModel<DemoViewModel.State, DemoViewModel.Action, DemoViewModel.Mutation, DemoViewModel.Effect>() {
        override var initState: State = State()
        private var jobSearch: Job? = null

        override fun handleAction(
            action: Action, state: State
        ) {
            when (action) {
                is Action.Increase -> {
                    viewModelScope.launch {
                        sendMutation(OnIncrease(action.count))
                    }
                }

                is Action.Decrease -> {
                    viewModelScope.launch {
                        sendMutation(OnDecrease(action.count))
                    }
                }

                is Action.Search -> {
                    jobSearch?.cancel()
                    jobSearch = viewModelScope.launch {
                        delay(action.timeTask)
                        sendMutation(UpdateSearch(action.text))
                    }
                }

                is Action.StringConcatenation -> {
                    viewModelScope.launch {
                        delay(action.timeTask)
                        sendMutation(AppendConcatenation(text = action.text))
                    }
                }
            }
        }

        override suspend fun handleMutation(
            mutation: Mutation, state: State
        ): State {
            return when (mutation) {
                is OnIncrease -> {
                    state.copy(number = state.number + mutation.count)
                }

                is OnDecrease -> {
                    state.copy(number = state.number - mutation.count)
                }

                is UpdateSearch -> {
                    state.copy(search = mutation.search)
                }

                is AppendConcatenation -> {
                    state.copy(
                        concatenation = listOfNotNull(
                            state.concatenation,
                            mutation.text
                        ).joinToString("_")
                    )
                }
            }
        }

        sealed class Action : BaseViewModel.VMAction {
            data class Increase(val count: Int) : Action()
            data class Decrease(val count: Int) : Action()
            data class Search(val text: String, val timeTask: Long) : Action()
            data class StringConcatenation(val text: String, val timeTask: Long) : Action()
        }

        sealed class Mutation : BaseViewModel.VMMutation {
            data class OnIncrease(val count: Int) : Mutation()
            data class OnDecrease(val count: Int) : Mutation()
            data class UpdateSearch(val search: String) : Mutation()
            data class AppendConcatenation(val text: String) : Mutation()
        }

        data class State(
            val number: Int = 0, val search: String? = null, val concatenation: String? = null
        ) : BaseViewModel.VMState

        sealed interface Effect : BaseViewModel.VMEffect {

        }
    }

}