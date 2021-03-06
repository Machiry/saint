/*
 * analysis_warning.h
 *
 *  Created on: 2014-06-24
 *      Author: Xavier N. Noumbissi
 */

#ifndef ANALYSIS_WARNING_H_
#define ANALYSIS_WARNING_H_

#include <llvm/IR/Value.h>
#include <llvm/IR/Function.h>
#include <string>
#include <vector>
#include <map>

using std::string;
using std::vector;
using std::map;

using namespace llvm;

namespace {

typedef enum {
  WARN_TYPE_UNDEFINED,
  TAINTED_VALUE_USE,
  FORMAT_TAINTED_VALUE_USE,
  FORMAT_STRING_VUL,
  FORMAT_STRING_MISSING_VUL
} WarnType;

class AnalysisWarning {

public:
	AnalysisWarning(Function *f, Function *sink, Value *val);
	~AnalysisWarning();

	static const unsigned INDENT_LENGTH;

	static void setValueTaintLineMap(map<Value *, unsigned> *valueToLine) {
		_valueToLine = valueToLine;
	}

	inline Function *getFunction(){ return _f; }

	inline Function *getSink(){ return _sink; }

	inline Value *getValue(){ return _val; }

	inline WarnType getWarnType(){ return _warnType; }

	inline unsigned getLine(){ return  _line; }

	inline int getFormatStrPos(){ return  _formatStrPos; }

	inline int getFuncParam(){ return _funcParam; }

	inline string getMessage(){ return _message; }

	inline void setFunction(Function *f){ _f = f; }

	inline void setSink(Function *sink){ _sink = sink; }

	inline void setValue(Value *v){ _val = v; }

	inline void setWarnType(WarnType wType){ _warnType = wType; }

	inline void setLine(unsigned line){ _line = line; }

	inline void setFormatStrPos(int formatStrPos){ _formatStrPos = formatStrPos; }

	inline void setFuncParam(unsigned funcParam){ _funcParam = funcParam; }

	inline void setMessage(string msg){ _message.assign(msg); }

	bool operator== (const AnalysisWarning* rhs);

	bool operator== (const AnalysisWarning& rhs);

	void print();

private:
	Function *_f;
	Function *_sink;
	Value *_val;
	WarnType _warnType;
	unsigned _line;
	int _formatStrPos;
	int _funcParam;
	string _message;

	static map<Value *, unsigned> *_valueToLine;
};

const unsigned AnalysisWarning::INDENT_LENGTH = 3;
map<Value *, unsigned> *AnalysisWarning::_valueToLine = 0;

inline AnalysisWarning::AnalysisWarning(Function *f, Function *sink, Value *val)
	:_f(f),
	 _sink(sink),
	 _val(val),
	 _warnType(WARN_TYPE_UNDEFINED),
	 _line(0),
	 _formatStrPos(-1),
	 _funcParam(-1)
{
	assert(_f && "Each created analysis issue (warning) must specify a function where it occurs!");
}

inline AnalysisWarning::~AnalysisWarning()
{
	_f = 0;
	_sink = 0;
	_val = 0;
	_line = 0;
	_funcParam = -1;
	_formatStrPos = -1;
	_message.clear();
}

bool AnalysisWarning::operator== (const AnalysisWarning& rhs)
{
	return this->_f == rhs._f &&
		   this->_sink == rhs._sink &&
		   this->_val == rhs._val &&
		   this->_line == rhs._line &&
		   this->_formatStrPos == rhs._formatStrPos &&
		   this->_warnType == rhs._warnType &&
		   this->_funcParam == rhs._funcParam;
}

bool AnalysisWarning::operator== (const AnalysisWarning* rhs)
{
	return this->_f == rhs->_f &&
		   this->_sink == rhs->_sink &&
		   this->_val == rhs->_val &&
		   this->_line == rhs->_line &&
		   this->_formatStrPos == rhs->_formatStrPos &&
		   this->_warnType == rhs->_warnType &&
		   this->_funcParam == rhs->_funcParam;
}

void AnalysisWarning::print()
{
	switch (_warnType) {
	case FORMAT_STRING_VUL:
		DEBUG_WITH_TYPE("saint-table", errs() << "[saint][fmtvul-1] Use of tainted format string (argument #"
				<< _funcParam << ") in sink function '"
				<< _sink->getName()
				<< ". [Line " << _line << "]\n");
		if (_val->hasName()) {
			DEBUG_WITH_TYPE("saint-table", errs().indent(INDENT_LENGTH) << _val->getName());
		}
		else {
			DEBUG_WITH_TYPE("saint-table", _val->print(errs().indent(INDENT_LENGTH)));
		}
		break;

	case FORMAT_TAINTED_VALUE_USE:
		DEBUG_WITH_TYPE("saint-table", errs() << "[saint][fmtvul-2] Parameter #"
				<< _funcParam
				<< " of sink call to '" << _sink->getName()
				<< "' is tainted. [Line " << _line << "]\n");
		if (_val->hasName())
			DEBUG_WITH_TYPE("saint-table", errs().indent(INDENT_LENGTH) << _val->getName());
		else
			DEBUG_WITH_TYPE("saint-table", _val->print(errs().indent(INDENT_LENGTH)));

		if (_valueToLine) {
			DEBUG_WITH_TYPE("saint-table", errs().indent(INDENT_LENGTH)
					<< "tainted at Line " << _valueToLine->at(_val));
		}
		break;

	case TAINTED_VALUE_USE:
		DEBUG_WITH_TYPE("saint-table", errs() << "[saint][tainted-value-use] Use of tainted value as parameter #"
				<< _funcParam << " in sink function '" << _sink->getName()
				<< "'. [Line " << _line << "]\n");
		if (_val->hasName())
			DEBUG_WITH_TYPE("saint-table", errs().indent(INDENT_LENGTH) << "(" << _val->getName() << ")");
		else
			DEBUG_WITH_TYPE("saint-table",_val->print(errs().indent(INDENT_LENGTH)));

		DEBUG_WITH_TYPE("saint-table", errs().indent(INDENT_LENGTH)
				<< " [Parameter #" << _funcParam << "] of '" << _sink->getName() << "' is tainted\n");
		break;

	case FORMAT_STRING_MISSING_VUL:
		DEBUG_WITH_TYPE("saint-table", errs() << "[saint][fmtvul-3] ");
		DEBUG_WITH_TYPE("saint-table", errs() << " Argument at position "
				<< _formatStrPos << " of function '" <<  _sink->getName()
				<< "' shall be a format string [Line " << _line << "] \n");

		DEBUG_WITH_TYPE("saint-table", errs().indent(INDENT_LENGTH) << "# Not => ");

		if (_val)	DEBUG_WITH_TYPE("saint-table", _val->print(errs()));

		DEBUG_WITH_TYPE("saint-table", errs() << "\n");
		break;

	default:
		break;
	}
}

} //namespace

#endif /* ANALYSIS_WARNING_H_ */
