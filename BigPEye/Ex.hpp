#pragma once

#include <sstream>
#include <exception>

class Ex : public std::exception
{
public:
  
  Ex() : exception{}, mStr{ std::make_shared<std::string>() }, mSS{ std::make_shared<std::stringstream>() }
  {
  }

  template<typename T>
  Ex & operator<<( T const& t )
  {
    *mSS << t;
    return *this;
  }

  char const* what() const override
  {
    mStr->append( mSS->str() );
    return mStr->c_str();
  }

private:
  std::shared_ptr<std::string> mStr;
  std::shared_ptr<std::stringstream> mSS;
};
