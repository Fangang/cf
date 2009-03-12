#include <cassert>
#include <iostream>
#include <sstream>
#include <map>
#include <vector>
#include <deque>
#include <string>

class XY;
class XYObject;

using namespace std;

class XYObject
{
  public:
    virtual void eval1(XY* xy) = 0;
    virtual string toString() = 0;
};

class XYNumber : public XYObject
{
  public:
    int mValue;

  public:
    XYNumber(int v = 0);
    virtual string toString();
    virtual void eval1(XY* xy);
};

class XYSymbol : public XYObject
{
  private:
    string mValue;

  public:
    XYSymbol(string v);
    virtual string toString();
    virtual void eval1(XY* xy);
};

class XYPrimitive : public XYObject
{
  private:
    string mName;

  public:
    XYPrimitive(string name);
    virtual string toString();
};

class XYAddition : public XYPrimitive
{
  public:
    XYAddition();
    virtual void eval1(XY* xy);
};

class XY {
  public:
    map<string, XYObject*> mEnv;
    vector<XYObject*> mX;
    deque<XYObject*> mY;

  public:
    void print();
    XY* eval1();
    XY* eval();
};

// XY
void XY::print() {
  for(int i=0; i < mX.size(); ++i) {
    cout << mX[i]->toString() << " ";
  }

  cout << "-> ";

  for(int i=0; i < mY.size(); ++i) {
    cout << mY[i]->toString() << " ";
  }

  cout << endl;
}

XY* XY::eval1() {
  assert(mY.size() > 0);

  XYObject* o = mY.front();
  assert(o);

  mY.pop_front();
  o->eval1(this);
  return this;
}

XY* XY::eval() {
  while (mY.size() > 0) {
    eval1();
  }
  return this;
}

// XYNumber
XYNumber::XYNumber(int v) : mValue(v) { }

string XYNumber::toString() {
  ostringstream s;
  s << mValue;
  return s.str();
}

void XYNumber::eval1(XY* xy) {
  xy->mX.push_back(this);
}

// XYSymbol
XYSymbol::XYSymbol(string v) : mValue(v) { }

string XYSymbol::toString() {
  ostringstream s;
  s << mValue;
  return s.str();
}

void XYSymbol::eval1(XY* xy) {
  xy->mX.push_back(this);
}

// XYPrimitive
XYPrimitive::XYPrimitive(string n) : mName(n) { }

string XYPrimitive::toString() {
  ostringstream s;
  s << mName;
  return s.str();
}

// XYAddition
XYAddition::XYAddition() : XYPrimitive("+") { }

void XYAddition::eval1(XY* xy) {
  assert(xy->mX.size() >= 2);
  XYNumber* rhs = dynamic_cast<XYNumber*>(xy->mX.back());
  assert(rhs);
  xy->mX.pop_back();

  XYNumber* lhs = dynamic_cast<XYNumber*>(xy->mX.back());
  assert(lhs);
  xy->mX.pop_back();

  xy->mX.push_back(new XYNumber(lhs->mValue + rhs->mValue));
}


void testXY() {
  XY* xy = new XY();
  XYObject* program1[] = {
    new XYNumber(1),
    new XYNumber(2),
    new XYNumber(3),
    new XYAddition(),
    new XYAddition()
  };

  xy->mY.insert(xy->mY.begin(), program1, program1 + (sizeof(program1)/sizeof(XYObject*)));

  while(xy->mY.size() > 0) {
    xy->print();
    xy->eval1();
  }

  xy->print();
  delete xy;
}

void runTests() {
  testXY();
}

int main() {
  runTests();

  return 0;
}

