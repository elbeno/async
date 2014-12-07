#include <async.h>

#include <cassert>
#include <iostream>
#include <string>

using namespace std;
using namespace async;

//------------------------------------------------------------------------------
// The identity function
template <typename T>
T id(T t)
{
  return t;
}

//------------------------------------------------------------------------------
// Functor laws

string ToString(int i)
{
  return to_string(i);
}

char FirstChar(string s)
{
  return s[0];
}

void testFmap()
{
  Async<int> i = pure(123);

  // identity
  {
    auto a = fmap(&id<int>, i);
    char result_a;
    a([&result_a] (char c) { result_a = c; });

    auto b = id(i);
    char result_b;
    b([&result_b] (char c) { result_b = c; });

    assert(result_a == result_b);
  }

  // composition
  {
    auto a = fmap(ToString, i);
    auto b = fmap(FirstChar, a);

    char result;
    b([&result] (char c) { result = c; });
    assert(result == '1');
  }
}

//------------------------------------------------------------------------------
// Multiple-argument apply

int add(int x, int y, int z)
{
  return x + y + z;
}

void testApply()
{
  auto x = async::fmap(add, async::pure(1));
  auto y = async::apply(x, async::pure(2));
  auto z = async::apply(y, async::pure(3));
  int result;
  z([&result] (int i) { result = i; });
  assert(result == 6);
}

//------------------------------------------------------------------------------
// Bind

Async<string> AsyncToString(int i)
{
  return [i] (std::function<void (string)> f) { f(to_string(i)); };
}

Async<char> AsyncFirstChar(string s)
{
  return [s] (std::function<void (char)> f) { f(s[0]); };
}

void testBind()
{
  auto a = async::pure(123) >= AsyncToString >= AsyncFirstChar;
  char result;
  a([&result] (char c) { result = c; });
  assert(result == '1');
}

//------------------------------------------------------------------------------
// Sequence

Async<char> AsyncChar()
{
  return [] (std::function<void (char)> f) { f('A'); };
}

Async<void> AsyncVoid()
{
  return [] (std::function<void ()> f) { f(); };
}

Async<void> AsyncIntToVoid(int)
{
  return [] (std::function<void ()> f) { f(); };
}

void testSequence()
{
  // sequence (Async<non-void> > non-void(void))
  {
    auto a = async::pure(123) > AsyncChar;
    char result;
    a([&result] (char c) { result = c; });
    assert(result == 'A');
  }

  // sequence (Async<non-void> > void(void))
  {
    auto a = async::pure(123) > AsyncVoid;
    a([] () {});
  }

  // sequence (Async<void> > non-void(void))
  {
    auto a = AsyncIntToVoid(123) > AsyncChar;
    char result;
    a([&result] (char c) { result = c; });
    assert(result == 'A');
  }

  // sequence (Async<void> > void(void))
  {
    auto a = AsyncIntToVoid(123) > AsyncVoid;
    a([] () {});
  }
}

//------------------------------------------------------------------------------
// AND

void testAnd()
{
  // AND two non-voids
  {
    auto a = AsyncChar() && AsyncChar();
    std::pair<char,char> result;
    a([&result] (const std::pair<char,char>& c) { result = c; });
    assert(result.first == 'A' && result.second == 'A');
  }

  // AND void and non-void
  {
    auto a = AsyncVoid() && AsyncChar();
    std::pair<Void,char> result;
    a([&result] (const std::pair<Void,char>& c) { result = c; });
    assert(result.second == 'A');
  }

  // AND non-void and void
  {
    auto a = AsyncChar() && AsyncVoid();
    std::pair<char,Void> result;
    a([&result] (const std::pair<char,Void>& c) { result = c; });
    assert(result.first == 'A');
  }

  // AND two voids
  {
    auto a = AsyncVoid() && AsyncVoid();
    a([] (const std::pair<Void,Void>&) {});
  }
}

//------------------------------------------------------------------------------
// OR

void testOr()
{
  // OR two non-voids
  {
    auto a = AsyncChar() || AsyncChar();
    a([] (const Either<char,char>&) {});
  }

  // OR void and non-void
  {
    auto a = AsyncVoid() || AsyncChar();
    a([] (const Either<Void,char>&) {});
  }

  // OR non-void and void
  {
    auto a = AsyncChar() || AsyncVoid();
    a([] (const Either<char,Void>&) {});
  }

  // OR two voids
  {
    auto a = AsyncVoid() || AsyncVoid();
    a([] (const Either<Void,Void>&) {});
  }
}

//------------------------------------------------------------------------------
// Performance tests: number of copies

struct CopyTest
{
  CopyTest() { ++s_constructCount; }
  ~CopyTest() { ++s_destructCount; }
  CopyTest(const CopyTest& other) { ++s_copyConstructCount; }
  CopyTest(CopyTest&& other) { ++s_moveConstructCount; }
  CopyTest& operator=(const CopyTest& other) { ++s_assignmentCount; return *this; }
  CopyTest& operator=(CopyTest&& other) { ++s_moveAssignmentCount; return *this; }

  static int s_constructCount;
  static int s_destructCount;
  static int s_copyConstructCount;
  static int s_moveConstructCount;
  static int s_assignmentCount;
  static int s_moveAssignmentCount;

  static void Reset()
  {
    s_constructCount = 0;
    s_destructCount = 0;
    s_copyConstructCount = 0;
    s_moveConstructCount = 0;
    s_assignmentCount = 0;
    s_moveAssignmentCount = 0;
  }

  static void Stats()
  {
    cout << s_constructCount << " constructs" << endl;
    cout << s_destructCount << " destructs" << endl;
    cout << s_copyConstructCount << " copy constructs" << endl;
    cout << s_moveConstructCount << " move constructs" << endl;
    cout << s_assignmentCount << " assignments" << endl;
    cout << s_moveAssignmentCount << " move assignments" << endl;
    Reset();
  }

  static void ExpectCopies(int n)
  {
    assert(s_copyConstructCount <= n);
    Reset();
  }
};

int CopyTest::s_constructCount;
int CopyTest::s_destructCount;
int CopyTest::s_copyConstructCount;
int CopyTest::s_moveConstructCount;
int CopyTest::s_assignmentCount;
int CopyTest::s_moveAssignmentCount;

Async<CopyTest> AsyncCopyTest()
{
  return [] (std::function<void (CopyTest)> f) { f(CopyTest()); };
}

CopyTest CopyTestId(const CopyTest& c)
{
  return c;
}

int NumCopies(const CopyTest& c)
{
  return c.s_copyConstructCount;
}

void testCopiesFmap()
{
  CopyTest::Reset();

  {
    auto a = AsyncCopyTest();
    CopyTest::ExpectCopies(0);
  }

  {
    auto a = AsyncCopyTest();
    a([] (const CopyTest&) {});
    CopyTest::ExpectCopies(0);
  }

  {
    auto a = AsyncCopyTest();
    auto b = fmap(NumCopies, a);
    b([] (int i) {});
    CopyTest::ExpectCopies(0);
  }

  {
    auto b = fmap(NumCopies, AsyncCopyTest());
    b([] (int i) {});
    CopyTest::ExpectCopies(0);
  }

  {
    auto a = AsyncCopyTest();
    auto b = fmap(NumCopies, fmap(CopyTestId, a));
    b([] (int i) {});
    CopyTest::ExpectCopies(1); // CopyTestId copies its argument
  }
}

void testCopiesPure()
{
  CopyTest::Reset();

  // rvalue
  {
    auto a = pure(CopyTest());
    a([] (const CopyTest&) {});
    CopyTest::ExpectCopies(0);
  }

  // lvalue
  {
    CopyTest c;
    auto a = pure(c);
    a([] (const CopyTest&) {});
    CopyTest::ExpectCopies(1); // pure must capture its argument
  }
}

int AddCopies2(const CopyTest& c1, const CopyTest& c2)
{
  return c1.s_copyConstructCount + c2.s_copyConstructCount;
}

int AddCopies3(const CopyTest& c1, const CopyTest& c2, const CopyTest& c3)
{
  return c1.s_copyConstructCount + c2.s_copyConstructCount + c3.s_copyConstructCount;
}

void testCopiesApply()
{
  CopyTest::Reset();

  cout << "Testing copies for apply" << endl;

  {
    auto b = apply(fmap(AddCopies2, pure(CopyTest())), pure(CopyTest()));
    b([] (int) {});
    cout << "After fmap(rvalue)/apply(rvalue) and retrieval:" << endl;
  }
  CopyTest::Stats();

  return;

  {
    auto a = pure(CopyTest());
    auto b = apply(fmap(AddCopies2, a), pure(CopyTest()));
    b([] (int) {});
    cout << "After fmap(lvalue)/apply(rvalue) and retrieval:" << endl;
  }
  CopyTest::Stats();

  {
    auto a = pure(CopyTest());
    auto b = apply(fmap(AddCopies2, a), a);
    b([] (int) {});
    cout << "After fmap(lvalue)/apply(lvalue) and retrieval:" << endl;
  }
  CopyTest::Stats();

  {
    auto b = apply(apply(fmap(AddCopies3, pure(CopyTest())), pure(CopyTest())), pure(CopyTest()));
    b([] (int) {});
    cout << "After fmap/apply/apply(rvalue) and retrieval:" << endl;
  }
  CopyTest::Stats();
}


//------------------------------------------------------------------------------
int main(int argc, char* argv[])
{
  testFmap();
  testApply();
  testBind();
  testSequence();
  testAnd();
  testOr();

  testCopiesFmap();
  testCopiesPure();
  testCopiesApply();

  return 0;
}
