#include <async.h>
#include <either.h>
#include <identity.h>

#include <cassert>
#include <iostream>
#include <string>
using namespace std;

//------------------------------------------------------------------------------
// a regular function
string AppendGo(const string& s)
{
  return s + "Go";
}

//------------------------------------------------------------------------------
// a monadic function

Either<int, string> MaybeAppendGo(const string& s)
{
  return Either<int, string>(s + "Go");
}

//------------------------------------------------------------------------------
void testEither()
{
  //------------------------------------------------------------------------------
  // Some examples

  // A simple Either
  Either<int, string> x = either::mreturn<int, string>("OK");
  cout << x << endl;

  // fmap a function into the Either
  Either<int, string> y = either::fmap(AppendGo, x);
  cout << y << endl;

  // fmap a monadic function - the result is a nested value
  Either<int, Either<int, string>> z1 = either::fmap(MaybeAppendGo, x);
  cout << z1 << endl;
  // join the result to get back to a non-nested value
  Either<int, string> z1j = either::mjoin(z1);
  cout << z1j << endl;

  // fmap . mjoin is equivalent to mbind
  Either<int, string> z = either::mbind(MaybeAppendGo, x);
  assert(z == z1j);

  //------------------------------------------------------------------------------
  // The monad laws

  // note that if m is an Either<L,R>, mreturn<decltype(m)::L, decltype(m)::R>
  // is identical to the single-argument (R) constructor for m

  // we can't take the address of a constructor, but we can take the address of
  // mreturn

  {
    // left identity
    // (return x) >>= f is equivalent to f x
    string s = "OK";
    auto x = either::mreturn<int, string>(s) >= MaybeAppendGo;
    auto y = MaybeAppendGo(s);
    assert(x == y);
  }

  {
    // right identity
    // m >>= return is equivalent to m
    auto m = either::mreturn<int, string>("OK");
    auto x = m >= either::mreturn<decltype(m)::L, decltype(m)::R>;
    assert(m == x);
  }

  {
    // associativity
    // (m >>= f) >>= g is equivalent to m >>= (\x -> f x >>= g)
    auto m = either::mreturn<int, string>("OK");
    auto x = (m >= MaybeAppendGo) >= MaybeAppendGo;
    auto y = m >= [] (const string& s) { return MaybeAppendGo(s) >= MaybeAppendGo; };
    assert(x == y);
  }

  {
    // test of >>
    auto m = either::mreturn<int, string>("OK");
    auto x = (m >= MaybeAppendGo) > [] ()
      { return decltype(m)("OK"); };
    //{ return either::mreturn<decltype(m)::L, decltype(m)::R>("OK"); };
    assert(m == x);
  }

}

//------------------------------------------------------------------------------
// a regular function
int Add1(int i)
{
  return i + 1;
}

//------------------------------------------------------------------------------
// a monadic function

Identity<int> MaybeAdd1(int i)
{
  return Identity<int>(i + 1);
}

//------------------------------------------------------------------------------
void testIdentity()
{
  //------------------------------------------------------------------------------
  // Some examples

  // A simple Identity
  Identity<int> x = identity::mreturn(1);
  cout << x << endl;

  // fmap a function into the Either
  Identity<int> y = identity::fmap(Add1, x);
  cout << y << endl;

  // fmap a monadic function - the result is a nested value
  Identity<Identity<int>> z1 = identity::fmap(MaybeAdd1, x);
  cout << z1 << endl;
  // join the result to get back to a non-nested value
  Identity<int> z1j = identity::mjoin(z1);
  cout << z1j << endl;

  // fmap . mjoin is equivalent to mbind
  Identity<int> z = identity::mbind(MaybeAdd1, x);
  assert(z == z1j);

  //------------------------------------------------------------------------------
  // The monad laws

  // note that if m is an Identity<T>, mreturn<decltype(m)::I> is identical to
  // the constructor for m, as above

  {
    // left identity
    // (return x) >>= f is equivalent to f x
    int n = 1;
    auto x = identity::mreturn<int>(n) >= MaybeAdd1;
    auto y = MaybeAdd1(n);
    assert(x == y);
  }

  {
    // right identity
    // m >>= return is equivalent to m
    auto m = identity::mreturn<int>(1);
    auto x = m >= identity::mreturn<decltype(m)::I>;
    assert(m == x);
  }

  {
    // associativity
    // (m >>= f) >>= g is equivalent to m >>= (\x -> f x >>= g)
    auto m = identity::mreturn<int>(1);
    auto x = (m >= MaybeAdd1) >= MaybeAdd1;
    auto y = m >= [] (int i) { return MaybeAdd1(i) >= MaybeAdd1; };
    assert(x == y);
  }

  {
    // test of >>
    auto m = identity::mreturn<int>(1);
    auto x = (m >= MaybeAdd1) > [] ()
      { return decltype(m)(1); };
      //{ return identity::mreturn<decltype(m)::I>(1); };
    assert(m == x);
  }

  {
    // test precedence/associativity
    auto m = identity::mreturn<int>(1);
    auto x = m >= MaybeAdd1 > [] ()
      { return decltype(m)(1); };
    assert(m == x);
  }

}

//------------------------------------------------------------------------------
string ToString(int i)
{
  return to_string(i);
}

char FirstChar(string s)
{
  return s[0];
}

Async<string> AsyncToString(int i)
{
  return [i] (std::function<void (string)> f) { f(to_string(i)); };
}

Async<char> AsyncFirstChar(string s)
{
  return [s] (std::function<void (char)> f) { f(s[0]); };
}

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

int add(int x, int y)
{
  return x + y;
}

int add2(int x, int y, int z)
{
  return x + y + z;
}

//------------------------------------------------------------------------------
void testAsync()
{
  // pure
  {
    Async<int> x = async::pure(100);
    x([] (int i) { cout << i << endl; });
  }

  // fmap
  {
    Async<int> x = async::pure(90);
    Async<char> y = async::fmap(FirstChar, async::fmap(ToString, x));
    y([] (char c) { cout << c << endl; });
  }

  // apply
  {
    Async<int> x = async::pure(80);
    Async<string> y = async::apply(async::pure(ToString), x);
    y([] (string s) { cout << s << endl; });
  }

  // fmap + apply (partial application)
  {
    Async<int> x = async::pure(90);
    auto y = async::fmap(add, x);
    auto z = async::apply(y, x);
    z([] (int i) { cout << i << endl; });
  }

  // apply (partial application)
  {
    auto x = async::pure(90);
    auto f = async::pure(add);
    auto y = async::apply(f, x);
    auto z = async::apply(y, x);
    z([] (int i) { cout << i << endl; });
  }
  {
    auto x = async::fmap(add2, async::pure(90));
    auto y = async::apply(x, async::pure(90));
    auto z = async::apply(y, async::pure(90));
    z([] (int i) { cout << i << endl; });
  }

  // bind
  {
    Async<string> x = AsyncToString(70);
    Async<char> y = async::bind(x, AsyncFirstChar);
    y([] (char c) { cout << c << endl; });
  }

  // multiple bind
  {
    (async::pure(60)
     >= AsyncToString
     >= AsyncFirstChar)([] (char c) { cout << c << endl; });
  }

  // sequence (Async<non-void> > non-void(void))
  {
    (async::pure(50)
     >= AsyncToString
     > AsyncChar)([] (char c) { cout << c << endl; });
  }

  // sequence (Async<non-void> > void(void))
  {
    (async::pure(50)
     >= AsyncToString
     > AsyncVoid)([] () { cout << "async void" << endl; });
  }

  // sequence (Async<void> > non-void(void))
  {
    (async::pure(50)
     >= AsyncIntToVoid
     > AsyncChar)([] (char c) { cout << c << endl; });
  }

  // sequence (Async<void> > void(void))
  {
    (async::pure(50)
     >= AsyncIntToVoid
     > AsyncVoid)([] () { cout << "async void 2" << endl; });
  }

  {
    (async::pure(40) && async::pure("foo"))
      ([] (std::pair<int, string> p) { cout << p.first << "," << p.second << endl; });
  }

  {
    (async::pure(30) || async::pure("foo"))
      ([] (Either<int, const char*> e) { cout << e << endl; });
    (async::zero() || async::pure("foo"))
      ([] (Either<async::Void, const char*> e) { cout << e << endl; });
    (async::pure(20) || async::zero())
      ([] (Either<int, async::Void> e) { cout << e << endl; });
  }

}

//------------------------------------------------------------------------------
int main(int argc, char* argv[])
{
  testAsync();
  testEither();
  testIdentity();
  cout << "All done." << endl;
  return 0;
}
