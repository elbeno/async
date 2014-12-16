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
  Async<short> i = pure(123);

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

  // lambdas
  {
    auto a = fmap([] (int n) { return to_string(n); }, i);
    string result;
    a([&result] (const string& s) { result = s; });
    assert(result == "123");
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
  // regular functions
  {
    auto x = fmap(add, pure(1));
    auto y = apply(x, pure(2));
    auto z = apply(y, pure(3));
    int result;
    z([&result] (int i) { result = i; });
    assert(result == 6);
  }

  // lambdas
  {
    auto x = fmap([] (int x, int y, int z) { return x + y + z; }, pure(1));
    auto y = apply(x, pure(2));
    auto z = apply(y, pure(3));
    int result;
    z([&result] (int i) { result = i; });
    assert(result == 6);
  }
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
  // regular functions
  {
    auto a = pure(123) >= AsyncToString >= AsyncFirstChar;
    char result;
    a([&result] (char c) { result = c; });
    assert(result == '1');
  }

  // lambdas
  {
    auto a = pure(123) >= [] (int i) -> Async<string> {
      return [i] (std::function<void (string)> f) { f(to_string(i)); }; };
    string result;
    a([&result] (const string& s) { result = s; });
    assert(result == "123");
  }

  // lvalue bind
  {
    auto a = pure(123);
    auto b = a >= AsyncToString;
    auto c = b >= AsyncFirstChar;
    c([] (char) {});
  }
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
    auto a = pure(123) > AsyncChar;
    char result;
    a([&result] (char c) { result = c; });
    assert(result == 'A');
  }

  // sequence (Async<non-void> > void(void))
  {
    auto a = pure(123) > AsyncVoid;
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

  // lambdas
  {
    auto a = AsyncChar() > [] () -> Async<void> {
      return [] (std::function<void ()> f) { f(); }; };
    a([] () {});
  }

  // lvalue sequence
  {
    auto a = pure(123);
    auto b = a > AsyncChar;
    b([] (char) {});
  }
}

//------------------------------------------------------------------------------
// AND

template <typename T>
Async<T> AsyncFirst(const std::pair<T,T>& p)
{
  return [p] (std::function<void (T)> f) { f(p.first); };
}

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

  // lvalues (two non-voids)
  {
    auto a1 = AsyncChar();
    auto a2 = AsyncChar();
    auto a = a1 && a2;
    a([] (std::pair<char,char>) {});
  }

  // lvalues (two voids)
  {
    auto a1 = AsyncVoid();
    auto a2 = AsyncVoid();
    auto a = a1 && a2;
    a([] (std::pair<Void,Void>) {});
  }

  // bind result
  {
    auto a = (AsyncChar() && AsyncChar()) >= AsyncFirst<char>;
    char result;
    a([&result] (char c) { result = c; });
    assert(result == 'A');
  }
}

//------------------------------------------------------------------------------
// OR

template <typename T>
Async<T> AsyncEither(const Either<T,T>& e)
{
  return [e] (std::function<void (T)> f) { f(e.isRight() ? e.m_right : e.m_left); };
}


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

  // lvalues (two non-voids)
  {
    auto a1 = AsyncChar();
    auto a2 = AsyncChar();
    auto a = a1 || a2;
    a([] (Either<char,char>) {});
  }

  // lvalues (two voids)
  {
    auto a1 = AsyncVoid();
    auto a2 = AsyncVoid();
    auto a = a1 || a2;
    a([] (Either<Void,Void>) {});
  }

  // bind result
  {
    auto a = (AsyncChar() || AsyncChar()) >= AsyncEither<char>;
    char result;
    a([&result] (char c) { result = c; });
    assert(result == 'A');
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
    assert(s_assignmentCount <= n);
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
    // CopyTestId copies its argument
    CopyTest::ExpectCopies(1);
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
    // pure must capture its lvalue argument
    CopyTest::ExpectCopies(1);
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

  // rvalues
  {
    auto b = apply(fmap(AddCopies2, pure(CopyTest())), pure(CopyTest()));
    b([] (int) {});
    CopyTest::ExpectCopies(0);
  }

  // move lvalues
  {
    auto a1 = pure(CopyTest());
    auto a2 = pure(CopyTest());
    auto b = apply(fmap(AddCopies2, std::move(a1)), std::move(a2));
    b([] (int) {});
    CopyTest::ExpectCopies(0);
  }

  // 1 lvalue
  {
    auto a = pure(CopyTest());
    auto b = apply(fmap(AddCopies2, a), pure(CopyTest()));
    b([] (int) {});
    CopyTest::ExpectCopies(1);
  }

  // 2 lvalues
  {
    auto a = pure(CopyTest());
    auto b = apply(fmap(AddCopies2, a), a);
    b([] (int) {});
    CopyTest::ExpectCopies(2);
  }

  // n-ary apply (rvalues)
  {
    auto b = apply(apply(fmap(AddCopies3, pure(CopyTest())), pure(CopyTest())), pure(CopyTest()));
    b([] (int) {});
    CopyTest::ExpectCopies(0);
  }

  // n-ary apply (lvalues)
  {
    auto a = pure(CopyTest());
    auto b = apply(apply(fmap(AddCopies3, a), a), a);
    b([] (int) {});
    CopyTest::ExpectCopies(3);
  }
}

Async<int> AsyncNumCopies(const CopyTest& c)
{
  int i = c.s_copyConstructCount;
  return [i] (std::function<void (int)> f) { f(i); };
}

void testCopiesBind()
{
  CopyTest::Reset();

  // rvalue
  {
    auto a = pure(CopyTest()) >= AsyncNumCopies;
    a([] (int i) {});
    CopyTest::ExpectCopies(0);
  }

  // lvalue
  {
    auto a = pure(CopyTest());
    auto b = a >= AsyncNumCopies;
    b([] (int i) {});
    CopyTest::ExpectCopies(1);
  }
}

void testCopiesAnd()
{
  CopyTest::Reset();

  // rvalues
  {
    auto a = pure(CopyTest()) && pure(CopyTest());
    a([] (const std::pair<CopyTest,CopyTest>&) {});
    CopyTest::ExpectCopies(0);
  }

  // lvalues
  {
    auto a1 = pure(CopyTest());
    auto a2 = pure(CopyTest());
    auto a = a1 && a2;
    a([] (const std::pair<CopyTest,CopyTest>&) {});
    CopyTest::ExpectCopies(2);
  }

  // lvalues (voids)
  {
    auto a1 = pure(CopyTest()) > AsyncVoid;
    auto a2 = pure(CopyTest()) > AsyncVoid;
    auto a = a1 && a2;
    a([] (const std::pair<Void,Void>&) {});
    CopyTest::ExpectCopies(2);
  }
}

void testCopiesOr()
{
  CopyTest::Reset();

  {
    auto a = pure(CopyTest()) || pure(CopyTest());
    a([] (const Either<CopyTest,CopyTest>&) {});
    CopyTest::ExpectCopies(0);
  }

  // lvalues
  {
    auto a1 = pure(CopyTest());
    auto a2 = pure(CopyTest());
    auto a = a1 || a2;
    a([] (const Either<CopyTest,CopyTest>&) {});
    CopyTest::ExpectCopies(2);
  }

  // lvalues (voids)
  {
    auto a1 = pure(CopyTest()) > AsyncVoid;
    auto a2 = pure(CopyTest()) > AsyncVoid;
    auto a = a1 || a2;
    a([] (const Either<Void,Void>&) {});
    CopyTest::ExpectCopies(2);
  }
}

//------------------------------------------------------------------------------
void testCopiesEither()
{
  CopyTest::Reset();

  // move construct right
  {
    Either<bool, CopyTest> e{CopyTest()};
    CopyTest::ExpectCopies(0);
  }

  // move construct left
  {
    Either<CopyTest, bool> e(CopyTest(), true);
    CopyTest::ExpectCopies(0);
  }

  // move assign left -> right
  {
    Either<bool, CopyTest> e1(true, true);
    Either<bool, CopyTest> e2{CopyTest()};
    CopyTest::Reset();
    e1 = std::move(e2);
    CopyTest::ExpectCopies(0);
  }

  // move assign right -> left
  {
    Either<CopyTest, bool> e1{true};
    Either<CopyTest, bool> e2(CopyTest(), true);
    CopyTest::Reset();
    e1 = std::move(e2);
    CopyTest::ExpectCopies(0);
  }

  // move assign right -> right
  {
    Either<bool, CopyTest> e1{CopyTest()};
    Either<bool, CopyTest> e2{CopyTest()};
    CopyTest::Reset();
    e1 = std::move(e2);
    CopyTest::ExpectCopies(0);
  }

  // move assign left -> left
  {
    Either<CopyTest, bool> e1(CopyTest(), true);
    Either<CopyTest, bool> e2(CopyTest(), true);
    CopyTest::Reset();
    e1 = std::move(e2);
    CopyTest::ExpectCopies(0);
  }

  // copy construct right
  CopyTest c;
  {
    Either<bool, CopyTest> e{c};
    CopyTest::ExpectCopies(1);
  }

  // copy construct left
  {
    Either<CopyTest, bool> e(c, true);
    CopyTest::ExpectCopies(1);
  }

  // copy assign left -> right
  {
    Either<bool, CopyTest> e1(true, true);
    Either<bool, CopyTest> e2{CopyTest()};
    CopyTest::Reset();
    e1 = e2;
    CopyTest::ExpectCopies(1);
  }

  // copy assign right -> left
  {
    Either<CopyTest, bool> e1{true};
    Either<CopyTest, bool> e2(CopyTest(), true);
    CopyTest::Reset();
    e1 = e2;
    CopyTest::ExpectCopies(1);
  }

  // copy assign right -> right
  {
    Either<bool, CopyTest> e1{CopyTest()};
    Either<bool, CopyTest> e2{CopyTest()};
    CopyTest::Reset();
    e1 = e2;
    CopyTest::ExpectCopies(1);
  }

  // copy assign left -> left
  {
    Either<CopyTest, bool> e1(CopyTest(), true);
    Either<CopyTest, bool> e2(CopyTest(), true);
    CopyTest::Reset();
    e1 = e2;
    CopyTest::ExpectCopies(1);
  }

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
  testCopiesBind();
  testCopiesAnd();
  testCopiesOr();

  testCopiesEither();

  return 0;
}
