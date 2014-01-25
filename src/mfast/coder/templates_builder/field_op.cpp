// Copyright (c) 2013, 2014, Huang-Ming Huang,  Object Computing, Inc.
// All rights reserved.
//
// This file is part of mFAST.
//
//     mFAST is free software: you can redistribute it and/or modify
//     it under the terms of the GNU Lesser General Public License as published by
//     the Free Software Foundation, either version 3 of the License, or
//     (at your option) any later version.
//
//     mFAST is distributed in the hope that it will be useful,
//     but WITHOUT ANY WARRANTY; without even the implied warranty of
//     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//     GNU General Public License for more details.
//
//     You should have received a copy of the GNU Lesser General Public License
//     along with mFast.  If not, see <http://www.gnu.org/licenses/>.
//
#include <cstddef>
#include <set>
#include "field_op.h"

namespace mfast
{
  namespace coder
  {


    std::ptrdiff_t
    hex2binary(const char* src, unsigned char* target)
    {
      unsigned char* dest = target;
      char c = 0;
      int shift_bits = 4;

      for (; *src != '\0'; ++src) {

        char tmp =0;

        if (*src >= '0' && *src <= '9') {
          tmp = (*src - '0');
        }
        else if (*src >= 'A' && *src <= 'F') {
          tmp = (*src - 'A') + '\x0A';
        }
        else if (*src >= 'a' && *src <= 'f') {
          tmp = (*src - 'a') + '\x0a';
        }
        else if (*src == ' ')
          continue;
        else
          return -1;

        c |= (tmp << shift_bits);

        if (shift_bits == 0) {
          *dest++ = c;
          c = 0;
          shift_bits = 4;
        }
        else
          shift_bits = 0;
      }

      if (shift_bits == 0)
        return -1;

      return dest - target;
    }

    struct cstr_compare
    {
      bool operator()(const char* lhs, const char* rhs) const
      {
        return strcmp(lhs, rhs) < 0;
      }

    };

    field_op::field_op(const decimal_field_instruction* inst,
                       const XMLElement*                element,
                       arena_allocator&                 alloc)
      : op_(inst->field_operator())
      , context_(inst->op_context())
      , initial_value_(inst->initial_value())
      , alloc_(&alloc)
    {
      if (element) {
        const XMLElement* field_op_element = find_field_op_element(*element);
        if (field_op_element) {
          parse_field_op(*field_op_element, alloc);
          const char* init_value_str = get_optional_attr(*field_op_element, "value", 0);

          if (init_value_str) {
            if (strcmp(element->Name(), "exponent") != 0 )
              initial_value_.set(boost::lexical_cast<int64_t>(init_value_str));
            else {
              short exp  = 128;
              try {
                exp = boost::lexical_cast<short>(init_value_str);
              }
              catch (...) {
              }

              if (exp > 63 || exp < -63) {
                BOOST_THROW_EXCEPTION(fast_dynamic_error("D11") << reason_info(std::string("Invalid exponent initial value: ") +  init_value_str ) );
              }
              initial_value_ = decimal_value_storage(0, static_cast<uint8_t>(exp)).storage_;
            }
          }
        }
      }
    }

    void field_op::set_init_value(const char* init_value_str,
                                  const byte_vector_field_instruction*)
    {
      if (init_value_str) {
        unsigned char* initial_value_buffer = static_cast<unsigned char*>(alloc_->allocate(strlen(init_value_str)/2+1));
        ptrdiff_t initial_value_len = hex2binary(init_value_str, initial_value_buffer);
        if (initial_value_len == -1) {
          BOOST_THROW_EXCEPTION(fast_dynamic_error("D11") << reason_info(std::string("Invalid byteVector initial value: ") +  init_value_str ) );
        }

        initial_value_ = byte_vector_value_storage(initial_value_buffer, initial_value_len).storage_;
      }
    }

    const XMLElement* field_op::find_field_op_element(const XMLElement& element) const
    {
      static const char* field_op_names[] = {
        "constant","default","copy","increment","delta","tail"
      };

      static std::set<const char*, cstr_compare> field_op_set (field_op_names, field_op_names + 6);

      for (const XMLElement* child = element.FirstChildElement(); child != 0; child = child->NextSiblingElement())
      {
        if (field_op_set.count(child->Name())) {
          return child;
        }
      }
      return 0;
    }

    void field_op::parse_field_op(const XMLElement& field_op_element,
                                  arena_allocator&  alloc)
    {
      typedef std::map<std::string,operator_enum_t> operator_map_t;
      static const operator_map_t operator_map =
        map_list_of ("none",operator_none)
          ("constant",operator_constant)
          ("delta",operator_delta)
          ("default",operator_default)
          ("copy",operator_copy)
          ("increment", operator_increment)
          ("tail",operator_tail);

      operator_map_t::const_iterator itr = operator_map.find(field_op_element.Name());
      if (itr == operator_map.end())
      {
        BOOST_THROW_EXCEPTION(fast_static_error("S1") << reason_info(std::string("Invalid field operator ") + field_op_element.Name()));
      }

      op_ = itr->second;

      const char* opContext_key = get_optional_attr(field_op_element, "key", context_ ? context_->key_ : 0);
      const char* opContext_dict = get_optional_attr(field_op_element, "dictionary", context_ ? context_->dictionary_ : 0 );
      const char* opContext_ns = get_optional_attr(field_op_element, "ns", context_ ? context_->ns_ : 0);

      if (opContext_key || opContext_dict || opContext_ns) {
        op_context_t* new_context = new (alloc) op_context_t;

        new_context->key_ = string_dup(opContext_key, alloc);
        new_context->ns_ = string_dup(opContext_ns, alloc);
        new_context->dictionary_ = string_dup(opContext_dict, alloc);
        context_ = new_context;
      }
    }

  } /* coder */

} /* mfast */
