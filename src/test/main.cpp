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
int main(int argc, char* argv[])
{
  testFmap();
  testApply();
  testBind();
  testSequence();
  testAnd();
  testOr();

  return 0;
}
