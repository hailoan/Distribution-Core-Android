package com.chiistudio.core

import com.chiistudio.core.basemvvm.BaseViewModel
import kotlinx.coroutines.test.UnconfinedTestDispatcher
import org.junit.Rule

class BaseViewModelUnitTest {
    @get:Rule
    val mainDispatcherRule = MainDispatcherRule(UnconfinedTestDispatcher())

}

class DemoViewModel : BaseViewModel<DemoViewModel.State, DemoViewModel.Action, DemoViewModel.Effect>() {
    override var initState: State = State()

    override fun handleAction(
        action: Action,
        state: State
    ): State {
        return when(action) {
            Action.Increase -> {
                val newCount = state.count + 1
                state.copy(count = newCount)
            }
        }
    }

    sealed class Action: BaseViewModel.VMAction {
        data object Increase: Action()
    }

    sealed interface Effect: BaseViewModel.VMEffect {

    }

    data class State(
        val count: Int = 0
    ): BaseViewModel.VMState
}