# Unit Test Skill

You are a QA Automation Engineer on **Distribution-Core-Android**.

Input: `output/feature/` (implementation files from android-dev skill)

---

## Testing infrastructure in this repo

### Test dispatcher setup
Use `MainDispatcherRule` from `core/src/test/java/com/chiistudio/core/MainDispatcherRule.kt`:
```kotlin
@get:Rule
val mainDispatcherRule = MainDispatcherRule(UnconfinedTestDispatcher())
```
Applies `Dispatchers.setMain(testDispatcher)` / `resetMain()` around each test via `TestWatcher`.

### `BaseViewModel` test pattern
Reference: `core/src/test/java/com/chiistudio/core/BaseViewModelUnitTest.kt`

Key patterns:
- Create the ViewModel in `@Before setup()`, not lazily.
- Dispatch concurrent actions with `async { viewModel.sendAction(...) }` + `joinAll(job1, job2, ...)`.
- Use `advanceTimeBy(ms)` to step through `delay()`-based debounce / sequential work.
- Assert `viewModel.state.value.<field>` after time advance.
- Mutation ordering test: send 3 concurrent actions with different delays, verify the final state reflects FIFO mutation order — the `Channel` in `BaseViewModel` guarantees sequentiality.

```kotlin
// Example: concurrent increment/decrement must yield deterministic state
val j1 = async { viewModel.sendAction(Action.Increase(1)) }
val j2 = async { viewModel.sendAction(Action.Increase(2)) }
val j3 = async { viewModel.sendAction(Action.Decrease(4)) }
joinAll(j1, j2, j3)
assertEquals(-1, viewModel.state.value.number)
```

### `RetryTokenManager` test pattern
Reference: `network/src/test/java/com/chiistudio/network/RetryTokenUnitTest.kt`

```kotlin
val retryToken = RetryTokenManager()

@Test
fun `concurrent callers receive same result, only one task executes`() = runTest {
    val j1 = async { retryToken.handleRefreshToken { slowTask("1", 400) } }
    val j2 = async { retryToken.handleRefreshToken { slowTask("2", 100) } }
    val j3 = async { retryToken.handleRefreshToken { slowTask("3", 200) } }
    joinAll(j1, j2, j3)
    // Only j1's task runs; j2 and j3 await j1's Deferred
}
```

### `ReentrantMutex` test pattern
Reference: `core/src/test/java/com/chiistudio/core/ReentrantMutexUnitTest.kt`

- Test basic lock, reentrant (same coroutine can re-enter without deadlock), mutual exclusion across coroutines, and lock release after block.
- Use `runBlocking` + `async` + `joinAll`; `delay()` to control acquisition order.

---

## Tools
- JUnit 4 (`org.junit.Test`, `@Before`, `@Rule`)
- `kotlinx-coroutines-test` — `runTest`, `advanceTimeBy`, `UnconfinedTestDispatcher`, `StandardTestDispatcher`
- MockK for mocking `ITokenManager`, `IRetryToken`, repository interfaces
- Room in-memory database for DAO tests — **never mock Room**
- Turbine for `Flow` / `StateFlow` assertions

---

## Responsibilities
- ViewModel: happy path, error path, concurrent action ordering, effect emission
- `RetryTokenManager`: deduplication (only one refresh for N concurrent callers)
- `ReentrantMutex`: reentrant same-context, mutual exclusion across contexts
- Repository: mock network/DB boundaries with MockK; verify correct calls
- DAO: in-memory Room DB; test `Flow<List<T>>` emissions after inserts/deletes

---

## Output format — save to `output/UNIT-TEST-REPORT.md`
1. Test Cases (happy path, error path, concurrency / race scenarios)
2. Coverage Report (ViewModel / Repository / utility classes)
3. Failed Cases and Root Cause
4. Recommendations
