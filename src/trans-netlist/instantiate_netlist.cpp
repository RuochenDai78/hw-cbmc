/*******************************************************************\

Module: 

Author: Daniel Kroening, kroening@kroening.com

\*******************************************************************/

#include <cstdlib>
#include <cassert>

#include <util/expr_util.h>
#include <util/std_expr.h>
#include <util/arith_tools.h>

#include "instantiate_netlist.h"

/*******************************************************************\

   Class: instantiate_bmc_mapt

 Purpose:

\*******************************************************************/

class instantiate_bmc_mapt:public boolbvt
{
public:
  instantiate_bmc_mapt(
    const namespacet &_ns,
    propt &solver,
    const bmc_mapt &_bmc_map,
    unsigned _current,
    unsigned _next):
    boolbvt(_ns, solver),
    bmc_map(_bmc_map),
    current(_current),
    next(_next)
  {
  }
  
  typedef boolbvt SUB;

  // overloading
  using boolbvt::get_literal;
  
  virtual literalt convert_bool(const exprt &expr);
  virtual literalt get_literal(const std::string &symbol, const unsigned bit);
  virtual void convert_bitvector(const exprt &expr, bvt &bv);
   
protected:   
  // disable smart variable allocation,
  // we already have literals for all variables
  virtual bool boolbv_set_equality_to_true(const equal_exprt &expr) { return true; }
  virtual bool set_equality_to_true(const equal_exprt &expr) { return true; }
  
  const bmc_mapt &bmc_map;
  unsigned current, next;
};

/*******************************************************************\

Function: instantiate_constraint

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

void instantiate_constraint(
  propt &solver,
  const bmc_mapt &bmc_map,
  const exprt &expr,
  unsigned current, unsigned next,
  const namespacet &ns,
  message_handlert &message_handler)
{
  instantiate_bmc_mapt i(ns, solver, bmc_map, current, next);

  i.set_message_handler(message_handler);

  try
  {
    i.set_to_true(expr);
  }

  catch(const char *err)
  {
    messaget(message_handler).error() << err << messaget::eom;
    exit(1);
  }

  catch(const std::string &err)
  {
    messaget(message_handler).error() << err << messaget::eom;
    exit(1);
  }
}

/*******************************************************************\

Function: instantiate_convert

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

literalt instantiate_convert(
  propt &solver,
  const bmc_mapt &bmc_map,
  const exprt &expr,
  unsigned current, unsigned next,
  const namespacet &ns,
  message_handlert &message_handler)
{
  instantiate_bmc_mapt i(ns, solver, bmc_map, current, next);

  i.set_message_handler(message_handler);

  try
  {
    return i.convert(expr);
  }

  catch(const char *err)
  {
    messaget(message_handler).error() << err << messaget::eom;
    exit(1);
  }

  catch(const std::string &err)
  {
    messaget(message_handler).error() << err << messaget::eom;
    exit(1);
  }
}

/*******************************************************************\

Function: instantiate_convert

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

void instantiate_convert(
  propt &solver,
  const bmc_mapt &bmc_map,
  const exprt &expr,
  unsigned current, unsigned next,
  const namespacet &ns,
  message_handlert &message_handler,
  bvt &bv)
{
  instantiate_bmc_mapt i(ns, solver, bmc_map, current, next);

  i.set_message_handler(message_handler);

  try
  {
    i.convert_bitvector(expr, bv);
  }

  catch(const char *err)
  {
    messaget(message_handler).error() << err << messaget::eom;
    exit(1);
  }

  catch(const std::string &err)
  {
    messaget(message_handler).error() << err << messaget::eom;
    exit(1);
  }
}

/*******************************************************************\

Function: instantiate_bmc_mapt::convert_bool

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

literalt instantiate_bmc_mapt::convert_bool(const exprt &expr)
{
  if(expr.id()==ID_symbol || expr.id()==ID_next_symbol)
  {
    bvt result;
    convert_bitvector(expr, result);

    if(result.size()!=1)
      throw "expected one-bit result";

    return result[0];
  }
  else if(expr.id()==ID_overlapped_implication)
  {
    // same as regular implication
    if(expr.operands().size()==2)
      return prop.limplies(convert_bool(expr.op0()),
                           convert_bool(expr.op1()));
  }
  else if(expr.id()==ID_non_overlapped_implication)
  {
    // right-hand side is shifted by one tick
    if(expr.operands().size()==2)
    {
      literalt lhs=convert_bool(expr.op0());
      unsigned old_current=current;
      unsigned old_next=next;
      literalt rhs=convert_bool(expr.op1());
      // restore      
      current=old_current;
      next=old_next;
      return prop.limplies(lhs, rhs);
    }
  }
  else if(expr.id()==ID_sva_cycle_delay) // ##[1:2] something
  {
    if(expr.operands().size()==3)
    {
      unsigned old_current=current;
      unsigned old_next=next;
      literalt result;

      if(expr.op1().is_nil())
      {
        mp_integer offset;
        if(to_integer(expr.op0(), offset))
          throw "failed to convert sva_cycle_delay offset";

        current=old_current+integer2long(offset);
        next=old_next+integer2long(offset);
        result=convert_bool(expr.op2());
      }
      else
      {
        mp_integer from, to;
        if(to_integer(expr.op0(), from) ||
           to_integer(expr.op1(), to))
          throw "failed to convert sva_cycle_delay offsets";
          
        // this is an 'or'
        bvt disjuncts;
        
        for(mp_integer offset=from; offset<to; ++offset)
        {
          current=old_current+integer2long(offset);
          next=old_next+integer2long(offset);
          disjuncts.push_back(convert_bool(expr.op2()));
        }
        
        result=prop.lor(disjuncts);
      }

      // restore      
      current=old_current;
      next=old_next;
      return result;
    }
  }
  else if(expr.id()==ID_sva_sequence_concatenation)
  {
    if(expr.operands().size()==2)
      return prop.land(convert_bool(expr.op0()),
                       convert_bool(expr.op1()));
  }

  return SUB::convert_bool(expr);
}

/*******************************************************************\

Function: instantiate_bmc_mapt::convert_bitvector

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

void instantiate_bmc_mapt::convert_bitvector(const exprt &expr, bvt &bv)
{
  if(expr.id()==ID_symbol || expr.id()==ID_next_symbol)
  {
    const irep_idt &identifier=expr.get(ID_identifier);

    unsigned width=boolbv_width(expr.type());

    if(width!=0)
    {
      unsigned timeframe=(expr.id()==ID_symbol)?current:next;

      bv.resize(width);

      for(unsigned i=0; i<width; i++)
        bv[i]=bmc_map.get(timeframe, identifier, i);

      return;
    }
  }

  return SUB::convert_bitvector(expr, bv);
}

/*******************************************************************\

Function: instantiate_bmc_mapt::get_literal

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

literalt instantiate_bmc_mapt::get_literal(
  const std::string &symbol,
  const unsigned bit)
{
  return bmc_map.get(current, symbol, bit);
}

/*******************************************************************\

   Class: instantiate_var_mapt

 Purpose:

\*******************************************************************/

class instantiate_var_mapt:public boolbvt
{
public:
  instantiate_var_mapt(
    const namespacet &_ns,
    propt &solver,
    const var_mapt &_var_map):
    boolbvt(_ns, solver),
    var_map(_var_map)
  {
  }
  
  typedef boolbvt SUB;

  // overloading
  using boolbvt::get_literal;
  
  virtual literalt convert_bool(const exprt &expr);
  virtual literalt get_literal(const std::string &symbol, const unsigned bit);
  virtual void convert_bitvector(const exprt &expr, bvt &bv);
   
protected:   
  // disable smart variable allocation,
  // we already have literals for all variables
  virtual bool boolbv_set_equality_to_true(const equal_exprt &expr) { return true; }
  virtual bool set_equality_to_true(const equal_exprt &expr) { return true; }
  
  const var_mapt &var_map;
};

/*******************************************************************\

Function: instantiate_constraint

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

void instantiate_constraint(
  propt &solver,
  const var_mapt &var_map,
  const exprt &expr,
  const namespacet &ns,
  message_handlert &message_handler)
{
  instantiate_var_mapt i(ns, solver, var_map);

  i.set_message_handler(message_handler);

  try
  {
    i.set_to_true(expr);
  }

  catch(const char *err)
  {
    messaget(message_handler).error() << err << messaget::eom;
    exit(1);
  }

  catch(const std::string &err)
  {
    messaget(message_handler).error() << err << messaget::eom;
    exit(1);
  }
}

/*******************************************************************\

Function: instantiate_convert

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

literalt instantiate_convert(
  propt &solver,
  const var_mapt &var_map,
  const exprt &expr,
  const namespacet &ns,
  message_handlert &message_handler)
{
  instantiate_var_mapt i(ns, solver, var_map);

  i.set_message_handler(message_handler);

  try
  {
    return i.convert(expr);
  }

  catch(const char *err)
  {
    messaget(message_handler).error() << err << messaget::eom;
    exit(1);
  }

  catch(const std::string &err)
  {
    messaget(message_handler).error() << err << messaget::eom;
    exit(1);
  }
}

/*******************************************************************\

Function: instantiate_convert

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

void instantiate_convert(
  propt &solver,
  const var_mapt &var_map,
  const exprt &expr,
  const namespacet &ns,
  message_handlert &message_handler,
  bvt &bv)
{
  instantiate_var_mapt i(ns, solver, var_map);

  i.set_message_handler(message_handler);

  try
  {
    i.convert_bitvector(expr, bv);
  }

  catch(const char *err)
  {
    messaget(message_handler).error() << err << messaget::eom;
    exit(1);
  }

  catch(const std::string &err)
  {
    messaget(message_handler).error() << err << messaget::eom;
    exit(1);
  }
}

/*******************************************************************\

Function: instantiate_var_mapt::convert_bool

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

literalt instantiate_var_mapt::convert_bool(const exprt &expr)
{
  if(expr.id()==ID_symbol || expr.id()==ID_next_symbol)
  {
    bvt result;
    convert_bitvector(expr, result);

    if(result.size()!=1)
      throw "expected one-bit result";

    return result[0];
  }

  return SUB::convert_bool(expr);
}

/*******************************************************************\

Function: instantiate_var_mapt::convert_bitvector

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

void instantiate_var_mapt::convert_bitvector(const exprt &expr, bvt &bv)
{
  if(expr.id()==ID_symbol || expr.id()==ID_next_symbol)
  {
    bool next=(expr.id()==ID_next_symbol);
    const irep_idt &identifier=expr.get(ID_identifier);

    unsigned width=boolbv_width(expr.type());

    if(width!=0)
    {
      bv.resize(width);

      for(unsigned i=0; i<width; i++)
        bv[i]=next?var_map.get_next(identifier, i)
                  :var_map.get_current(identifier, i);

      return;
    }
  }

  return SUB::convert_bitvector(expr, bv);
}

/*******************************************************************\

Function: instantiate_var_mapt::get_literal

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

literalt instantiate_var_mapt::get_literal(
  const std::string &symbol,
  const unsigned bit)
{
  return var_map.get_current(symbol, bit);
}
