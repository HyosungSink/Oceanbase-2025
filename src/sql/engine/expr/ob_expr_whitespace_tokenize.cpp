/**
 * Copyright (c) 2025 OceanBase
 * OceanBase CE is licensed under Mulan PubL v2.
 * You can use this software according to the terms and conditions of the Mulan PubL v2.
 * You may obtain a copy of Mulan PubL v2 at:
 *          http://license.coscl.org.cn/MulanPubL-2.0
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PubL v2 for more details.
 */

#define USING_LOG_PREFIX SQL_ENG

#include "sql/engine/expr/ob_expr_whitespace_tokenize.h"

#include "lib/charset/ob_charset.h"
#include "lib/udt/ob_array_type.h"
#include "lib/udt/ob_collection_type.h"
#include "sql/engine/expr/ob_array_expr_utils.h"
#include "sql/engine/expr/ob_expr_lob_utils.h"
#include "sql/engine/ob_exec_context.h"

using namespace oceanbase::common;

namespace oceanbase
{
namespace sql
{

ObExprWhitespaceTokenize::ObExprWhitespaceTokenize(ObIAllocator &alloc)
  : ObFuncExprOperator(alloc,
                       T_FUNC_SYS_WHITESPACE_TOKENIZE,
                       N_WHITESPACE_TOKENIZE,
                       1,
                       VALID_FOR_GENERATED_COL,
                       NOT_ROW_DIMENSION)
{
}

int ObExprWhitespaceTokenize::calc_result_typeN(ObExprResType &type,
                                                ObExprResType *types,
                                                int64_t param_num,
                                                common::ObExprTypeCtx &type_ctx) const
{
  int ret = OB_SUCCESS;
  uint16_t subschema_id = UINT16_MAX;
  ObDataType res_data_type;
  ObSQLSessionInfo *session = const_cast<ObSQLSessionInfo *>(type_ctx.get_session());
  ObExecContext *exec_ctx = OB_ISNULL(session) ? nullptr : session->get_cur_exec_ctx();

  if (OB_UNLIKELY(param_num != 1)) {
    ret = OB_ERR_PARAM_SIZE;
    LOG_USER_ERROR(OB_ERR_PARAM_SIZE, static_cast<int>(strlen(N_WHITESPACE_TOKENIZE)), N_WHITESPACE_TOKENIZE);
  } else if (OB_ISNULL(exec_ctx)) {
    ret = OB_ERR_UNEXPECTED;
    LOG_WARN("exec ctx is null", K(ret));
  }

  for (int64_t i = 0; OB_SUCC(ret) && i < param_num; ++i) {
    if (ob_is_null(types[i].get_type())) {
      // do nothing
    } else if (ob_is_text(types[i].get_type(), types[i].get_collation_type())) {
      types[i].set_calc_collation_type(CS_TYPE_UTF8MB4_BIN);
    } else if (ob_is_varchar_char_type(types[i].get_type(), types[i].get_collation_type())) {
      types[i].set_calc_collation_type(CS_TYPE_UTF8MB4_BIN);
    } else {
      ret = OB_ERR_INVALID_TYPE_FOR_OP;
      LOG_USER_ERROR(OB_ERR_INVALID_TYPE_FOR_OP, "VARCHAR", ob_obj_type_str(types[i].get_type()));
    }
  }

  if (OB_SUCC(ret)) {
    // result type: ARRAY<VARCHAR>
    ObObjMeta meta;
    meta.set_varchar();
    res_data_type.set_meta_type(meta);
    // cap to a reasonable default; same with string_to_array
    res_data_type.set_length(OB_MAX_VARCHAR_LENGTH / 4);
    if (OB_FAIL(exec_ctx->get_subschema_id_by_collection_elem_type(ObNestedType::OB_ARRAY_TYPE,
                                                                   res_data_type,
                                                                   subschema_id))) {
      LOG_WARN("failed to get collection subschema id", K(ret));
    } else {
      type.set_collection(subschema_id);
    }
  }

  return ret;
}

int ObExprWhitespaceTokenize::eval_whitespace_tokenize(const ObExpr &expr,
                                                       ObEvalCtx &ctx,
                                                       ObDatum &res)
{
  int ret = OB_SUCCESS;
  ObEvalCtx::TempAllocGuard tmp_alloc_g(ctx);
  ObArenaAllocator &tmp_alloc = tmp_alloc_g.get_allocator();
  const uint16_t subschema_id = expr.obj_meta_.get_subschema_id();
  const ObCollationType cs_type = expr.args_[0]->datum_meta_.cs_type_;

  ObDatum *text_datum = nullptr;
  if (OB_FAIL(expr.args_[0]->eval(ctx, text_datum))) {
    LOG_WARN("eval input text failed", K(ret));
  } else if (text_datum->is_null()) {
    res.set_null();
  } else {
    // read real data to handle LOB/text
    ObString input;
    if (OB_FAIL(ObTextStringHelper::read_real_string_data(tmp_alloc,
                                                          *text_datum,
                                                          expr.args_[0]->datum_meta_,
                                                          expr.args_[0]->obj_meta_.has_lob_header(),
                                                          input))) {
      LOG_WARN("failed to get real string data", K(ret));
    } else {
      const ObCharsetInfo *cs = ObCharset::get_charset(cs_type);
      ObIArrayType *arr_obj = nullptr;
      ObArrayBinary *binary_array = nullptr;
      if (OB_FAIL(ObArrayExprUtils::construct_array_obj(tmp_alloc, ctx, subschema_id, arr_obj, false))) {
        LOG_WARN("construct array obj failed", K(ret), K(subschema_id));
      } else if (OB_ISNULL(binary_array = static_cast<ObArrayBinary *>(arr_obj))) {
        ret = OB_ERR_UNEXPECTED;
        LOG_WARN("binary array is null", K(ret), K(subschema_id));
      } else {
        // simple whitespace tokenizer: split by one or more whitespace characters
        const char *buf = input.ptr();
        const int64_t len = input.length();
        int64_t i = 0;
        // skip leading spaces
        while (i < len && (cs != nullptr ? ob_isspace(cs, buf[i]) : isspace(static_cast<unsigned char>(buf[i])))) ++i;
        while (OB_SUCC(ret) && i < len) {
          const int64_t start = i;
          // advance to next whitespace
          while (i < len && !(cs != nullptr ? ob_isspace(cs, buf[i]) : isspace(static_cast<unsigned char>(buf[i])))) ++i;
          const int64_t tok_len = i - start;
          if (tok_len > 0) {
            if (OB_FAIL(binary_array->push_back(ObString(static_cast<int32_t>(tok_len), buf + start)))) {
              LOG_WARN("failed to push token", K(ret));
            }
          }
          // skip subsequent whitespace
          while (i < len && (cs != nullptr ? ob_isspace(cs, buf[i]) : isspace(static_cast<unsigned char>(buf[i])))) ++i;
        }

        if (OB_SUCC(ret)) {
          ObString res_str;
          if (OB_FAIL(ObArrayExprUtils::set_array_res(arr_obj,
                                                      arr_obj->get_raw_binary_len(),
                                                      expr,
                                                      ctx,
                                                      res_str))) {
            LOG_WARN("get array binary string failed", K(ret));
          } else {
            res.set_string(res_str);
          }
        }
      }
    }
  }
  return ret;
}

int ObExprWhitespaceTokenize::cg_expr(ObExprCGCtx &expr_cg_ctx,
                                      const ObRawExpr &raw_expr,
                                      ObExpr &rt_expr) const
{
  UNUSED(expr_cg_ctx);
  UNUSED(raw_expr);
  rt_expr.eval_func_ = eval_whitespace_tokenize;
  return OB_SUCCESS;
}

} // namespace sql
} // namespace oceanbase

