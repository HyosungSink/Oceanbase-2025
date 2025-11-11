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
 *
 * whitespace_tokenize(text) -> array<char/varchar>
 */

#ifndef OCEANBASE_SQL_ENGINE_EXPR_OB_EXPR_WHITESPACE_TOKENIZE_H_
#define OCEANBASE_SQL_ENGINE_EXPR_OB_EXPR_WHITESPACE_TOKENIZE_H_

#include "sql/engine/expr/ob_expr_operator.h"

namespace oceanbase
{
namespace sql
{

class ObExprWhitespaceTokenize : public ObFuncExprOperator
{
public:
  explicit ObExprWhitespaceTokenize(common::ObIAllocator &alloc);
  ~ObExprWhitespaceTokenize() override {}

  int calc_result_typeN(ObExprResType &type,
                        ObExprResType *types,
                        int64_t param_num,
                        common::ObExprTypeCtx &type_ctx) const override;
  int calc_result_type1(ObExprResType &type,
                        ObExprResType &text,
                        common::ObExprTypeCtx &type_ctx) const override;

  static int eval_whitespace_tokenize(const ObExpr &expr,
                                      ObEvalCtx &ctx,
                                      ObDatum &res);

  int cg_expr(ObExprCGCtx &expr_cg_ctx,
              const ObRawExpr &raw_expr,
              ObExpr &rt_expr) const override;

private:
  DISALLOW_COPY_AND_ASSIGN(ObExprWhitespaceTokenize);
};

} // namespace sql
} // namespace oceanbase

#endif // OCEANBASE_SQL_ENGINE_EXPR_OB_EXPR_WHITESPACE_TOKENIZE_H_
