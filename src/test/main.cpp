#include <either.h>
#include <identity.h>

#include <cassert>
#include <iostream>
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
    auto x = either::mreturn<int, string>(s) >>= MaybeAppendGo;
    auto y = MaybeAppendGo(s);
    assert(x == y);
  }

  {
    // right identity
    // m >>= return is equivalent to m
    auto m = either::mreturn<int, string>("OK");
    auto x = m >>= either::mreturn<decltype(m)::L, decltype(m)::R>;
    assert(m == x);
  }

  {
    // associativity
    // (m >>= f) >>= g is equivalent to m >>= (\x -> f x >>= g)
    auto m = either::mreturn<int, string>("OK");
    auto x = (m >>= MaybeAppendGo) >>= MaybeAppendGo;
    auto y = m >>= [] (const string& s) { return MaybeAppendGo(s) >>= MaybeAppendGo; };
    assert(x == y);
  }

  {
    // test of >>
    auto m = either::mreturn<int, string>("OK");
    auto x = (m >>= MaybeAppendGo) >> [] ()
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
    auto x = identity::mreturn<int>(n) >>= MaybeAdd1;
    auto y = MaybeAdd1(n);
    assert(x == y);
  }

  {
    // right identity
    // m >>= return is equivalent to m
    auto m = identity::mreturn<int>(1);
    auto x = m >>= identity::mreturn<decltype(m)::I>;
    assert(m == x);
  }

  {
    // associativity
    // (m >>= f) >>= g is equivalent to m >>= (\x -> f x >>= g)
    auto m = identity::mreturn<int>(1);
    auto x = (m >>= MaybeAdd1) >>= MaybeAdd1;
    auto y = m >>= [] (int i) { return MaybeAdd1(i) >>= MaybeAdd1; };
    assert(x == y);
  }

  {
    // test of >>
    auto m = identity::mreturn<int>(1);
    auto x = (m >>= MaybeAdd1) >> [] ()
      { return decltype(m)(1); };
      //{ return identity::mreturn<decltype(m)::I>(1); };
    assert(m == x);
  }
}

//------------------------------------------------------------------------------
int main(int argc, char* argv[])
{
  testEither();
  testIdentity();
  cout << "All done." << endl;
  return 0;
}
