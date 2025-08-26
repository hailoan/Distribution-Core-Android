package com.chiistudio.core

import com.chiistudio.core.basemvvm.BaseViewModel
import kotlinx.coroutines.test.UnconfinedTestDispatcher
import org.junit.Rule

class BaseViewModelUnitTest {
    @get:Rule
    val mainDispatcherRule = MainDispatcherRule(UnconfinedTestDispatcher())

}

class DemoViewModel :
    BaseViewModel<DemoViewModel.State, DemoViewModel.Action, DemoViewModel.Effect>() {
    override var initState: State = State()

    override fun handleAction(action: Action, state: State, emitState: ((State) -> State) -> Unit) {
        when (action) {
            Action.Increase -> {
                val newCount = state.count + 1
                emitState {
                    it.copy(newCount)
                }
            }

            Action.Decrease -> {
                val newCount = state.count - 1
                emitState {
                    it.copy(newCount)
                }
            }
        }
    }

    sealed class Action : BaseViewModel.VMAction {
        data object Increase : Action()
        data object Decrease : Action()
    }

    sealed interface Effect : BaseViewModel.VMEffect {

    }

    data class State(
        val count: Int = 0
    ) : BaseViewModel.VMState
}