// quick-lint-js finds bugs in JavaScript programs.
// Copyright (C) 2020  Matthew Glazar
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#include <algorithm>
#include <optional>
#include <quick-lint-js/assert.h>
#include <quick-lint-js/char8.h>
#include <quick-lint-js/error.h>
#include <quick-lint-js/language.h>
#include <quick-lint-js/lex.h>
#include <quick-lint-js/lint.h>
#include <quick-lint-js/optional.h>
#include <vector>

namespace quick_lint_js {
linter::linter(error_reporter *error_reporter)
    : error_reporter_(error_reporter) {
  this->scopes_.emplace_back();
  this->scopes_.emplace_back();
  scope &global_scope = this->scopes_[0];
  scope &module_scope = this->scopes_[1];

  const char8 *writable_global_variables[] = {
      // ECMA-262 18.1 Value Properties of the Global Object
      u8"globalThis",

      // ECMA-262 18.2 Function Properties of the Global Object
      u8"decodeURI",
      u8"decodeURIComponent",
      u8"encodeURI",
      u8"encodeURIComponent",
      u8"eval",
      u8"isFinite",
      u8"isNaN",
      u8"parseFloat",
      u8"parseInt",

      // ECMA-262 18.3 Constructor Properties of the Global Object
      u8"Array",
      u8"ArrayBuffer",
      u8"BigInt",
      u8"BigInt64Array",
      u8"BigUint64Array",
      u8"Boolean",
      u8"DataView",
      u8"Date",
      u8"Error",
      u8"EvalError",
      u8"Float32Array",
      u8"Float64Array",
      u8"Function",
      u8"Int16Array",
      u8"Int32Array",
      u8"Int8Array",
      u8"Map",
      u8"Number",
      u8"Object",
      u8"Promise",
      u8"Proxy",
      u8"RangeError",
      u8"ReferenceError",
      u8"RegExp",
      u8"Set",
      u8"SharedArrayBuffer",
      u8"String",
      u8"Symbol",
      u8"SyntaxError",
      u8"TypeError",
      u8"URIError",
      u8"Uint16Array",
      u8"Uint32Array",
      u8"Uint8Array",
      u8"Uint8ClampedArray",
      u8"WeakMap",
      u8"WeakSet",

      // ECMA-262 18.4 Other Properties of the Global Object
      u8"Atomics",
      u8"JSON",
      u8"Math",
      u8"Reflect",

      // Node.js
      u8"Buffer",
      u8"GLOBAL",
      u8"Intl",
      u8"TextDecoder",
      u8"TextEncoder",
      u8"URL",
      u8"URLSearchParams",
      u8"WebAssembly",
      u8"clearImmediate",
      u8"clearInterval",
      u8"clearTimeout",
      u8"console",
      u8"escape",
      u8"global",
      u8"process",
      u8"queueMicrotask",
      u8"root",
      u8"setImmediate",
      u8"setInterval",
      u8"setTimeout",
      u8"unescape",
  };

  for (const char8 *global_variable : writable_global_variables) {
    global_scope.add_predefined_variable_declaration(global_variable,
                                                     variable_kind::_function);
  }

  const char8 *non_writable_global_variables[] = {
      // ECMA-262 18.1 Value Properties of the Global Object
      u8"Infinity",
      u8"NaN",
      u8"undefined",
  };
  for (const char8 *global_variable : non_writable_global_variables) {
    global_scope.add_predefined_variable_declaration(global_variable,
                                                     variable_kind::_const);
  }

  const char8 *writable_module_variables[] = {
      // Node.js
      u8"__dirname", u8"__filename", u8"exports", u8"module", u8"require",
  };

  for (const char8 *module_variable : writable_module_variables) {
    module_scope.add_predefined_variable_declaration(module_variable,
                                                     variable_kind::_function);
  }
}

void linter::visit_enter_block_scope() { this->scopes_.emplace_back(); }

void linter::visit_enter_class_scope() {}

void linter::visit_enter_for_scope() { this->scopes_.emplace_back(); }

void linter::visit_enter_function_scope() { this->scopes_.emplace_back(); }

void linter::visit_enter_function_scope_body() {
  this->propagate_variable_uses_to_parent_scope(
      /*allow_variable_use_before_declaration=*/true,
      /*consume_arguments=*/true);
}

void linter::visit_enter_named_function_scope(identifier function_name) {
  scope &current_scope = this->scopes_.emplace_back();
  current_scope.function_expression_declaration = declared_variable{
      .kind = variable_kind::_function,
      .declaration = function_name,
      .declaration_scope = declared_variable_scope::declared_in_current_scope,
  };
}

void linter::visit_exit_block_scope() {
  assert(!this->scopes_.empty());
  this->propagate_variable_uses_to_parent_scope(
      /*allow_variable_use_before_declaration=*/false,
      /*consume_arguments=*/false);
  this->propagate_variable_declarations_to_parent_scope();
  this->scopes_.pop_back();
}

void linter::visit_exit_class_scope() {}

void linter::visit_exit_for_scope() {
  QLJS_ASSERT(!this->scopes_.empty());
  this->propagate_variable_uses_to_parent_scope(
      /*allow_variable_use_before_declaration=*/false,
      /*consume_arguments=*/false);
  this->propagate_variable_declarations_to_parent_scope();
  this->scopes_.pop_back();
}

void linter::visit_exit_function_scope() {
  QLJS_ASSERT(!this->scopes_.empty());
  this->propagate_variable_uses_to_parent_scope(
      /*allow_variable_use_before_declaration=*/true,
      /*consume_arguments=*/true);
  this->scopes_.pop_back();
}

void linter::visit_property_declaration(identifier) {}

void linter::visit_variable_declaration(identifier name, variable_kind kind) {
  this->declare_variable(
      /*scope=*/this->scopes_.back(),
      /*name=*/name,
      /*kind=*/kind,
      /*variable_scope=*/declared_variable_scope::declared_in_current_scope);
}

void linter::declare_variable(scope &scope, identifier name, variable_kind kind,
                              declared_variable_scope declared_scope) {
  if (declared_scope == declared_variable_scope::declared_in_descendant_scope) {
    QLJS_ASSERT(kind == variable_kind::_function ||
                kind == variable_kind::_var);
  }

  this->report_error_if_variable_declaration_conflicts_in_scope(
      scope, name, kind, declared_scope);

  const declared_variable *declared =
      scope.add_variable_declaration(name, kind, declared_scope);

  auto erase_if = [](auto &variables, auto predicate) {
    variables.erase(
        std::remove_if(variables.begin(), variables.end(), predicate),
        variables.end());
  };
  erase_if(scope.variables_used, [&](const used_variable &used_var) {
    if (name.string_view() == used_var.name.string_view()) {
      if (kind == variable_kind::_class || kind == variable_kind::_const ||
          kind == variable_kind::_let) {
        switch (used_var.kind) {
          case used_variable_kind::assignment:
            this->report_error_if_assignment_is_illegal(declared,
                                                        used_var.name);
            this->error_reporter_
                ->report_error_assignment_before_variable_declaration(
                    used_var.name, name);
            break;
          case used_variable_kind::_typeof:
          case used_variable_kind::use:
            this->error_reporter_
                ->report_error_variable_used_before_declaration(used_var.name,
                                                                name);
            break;
        }
      }
      return true;
    } else {
      return false;
    }
  });
  erase_if(scope.variables_used_in_descendant_scope,
           [&](const used_variable &used_var) {
             switch (used_var.kind) {
               case used_variable_kind::assignment:
                 this->report_error_if_assignment_is_illegal(declared,
                                                             used_var.name);
                 break;
               case used_variable_kind::_typeof:
               case used_variable_kind::use:
                 break;
             }
             return name.string_view() == used_var.name.string_view();
           });
}

void linter::visit_variable_assignment(identifier name) {
  QLJS_ASSERT(!this->scopes_.empty());
  scope &current_scope = this->scopes_.back();
  const declared_variable *var = current_scope.find_declared_variable(name);
  if (var) {
    this->report_error_if_assignment_is_illegal(var, name);
  } else {
    current_scope.variables_used.emplace_back(name,
                                              used_variable_kind::assignment);
  }
}

void linter::visit_variable_typeof_use(identifier name) {
  this->visit_variable_use(name, used_variable_kind::_typeof);
}

void linter::visit_variable_use(identifier name) {
  this->visit_variable_use(name, used_variable_kind::use);
}

void linter::visit_variable_use(identifier name, used_variable_kind use_kind) {
  QLJS_ASSERT(!this->scopes_.empty());
  scope &current_scope = this->scopes_.back();
  bool variable_is_declared =
      current_scope.find_declared_variable(name) != nullptr;
  if (!variable_is_declared) {
    current_scope.variables_used.emplace_back(name, use_kind);
  }
}

void linter::visit_end_of_module() {
  this->propagate_variable_uses_to_parent_scope(
      /*allow_variable_use_before_declaration=*/false,
      /*consume_arguments=*/false);
  this->scopes_.pop_back();

  QLJS_ASSERT(this->scopes_.size() == 1);
  scope &global_scope = this->scopes_.back();

  std::vector<identifier> typeof_variables;
  for (const used_variable &used_var : global_scope.variables_used) {
    if (used_var.kind == used_variable_kind::_typeof) {
      typeof_variables.emplace_back(used_var.name);
    }
  }
  for (const used_variable &used_var :
       global_scope.variables_used_in_descendant_scope) {
    if (used_var.kind == used_variable_kind::_typeof) {
      typeof_variables.emplace_back(used_var.name);
    }
  }
  auto is_variable_declared_by_typeof = [&](const used_variable &var) -> bool {
    return std::find_if(typeof_variables.begin(), typeof_variables.end(),
                        [&](const identifier &typeof_variable) {
                          return typeof_variable.string_view() ==
                                 var.name.string_view();
                        }) != typeof_variables.end();
  };
  auto is_variable_declared = [&](const used_variable &var) -> bool {
    return global_scope.find_declared_variable(var.name) ||
           is_variable_declared_by_typeof(var);
  };

  for (const used_variable &used_var : global_scope.variables_used) {
    if (!is_variable_declared(used_var)) {
      switch (used_var.kind) {
        case used_variable_kind::assignment:
          this->error_reporter_->report_error_assignment_to_undeclared_variable(
              used_var.name);
          break;
        case used_variable_kind::use:
          this->error_reporter_->report_error_use_of_undeclared_variable(
              used_var.name);
          break;
        case used_variable_kind::_typeof:
          // 'typeof foo' is often used to detect if the variable 'foo' is
          // declared. Do not report that the variable is undeclared.
          break;
      }
    }
  }
  for (const used_variable &used_var :
       global_scope.variables_used_in_descendant_scope) {
    if (!is_variable_declared(used_var)) {
      // TODO(strager): Should we check used_var.kind?
      this->error_reporter_->report_error_use_of_undeclared_variable(
          used_var.name);
    }
  }
}

void linter::propagate_variable_uses_to_parent_scope(
    bool allow_variable_use_before_declaration, bool consume_arguments) {
  QLJS_ASSERT(this->scopes_.size() >= 2);
  scope &current_scope = this->scopes_[this->scopes_.size() - 1];
  scope &parent_scope = this->scopes_[this->scopes_.size() - 2];

  auto is_current_scope_function_name = [&](const used_variable &var) {
    if (!current_scope.function_expression_declaration.has_value()) {
      return false;
    }
    const declared_variable &decl =
        *current_scope.function_expression_declaration;
    QLJS_ASSERT(decl.declaration.has_value());
    return decl.declaration->string_view() == var.name.string_view();
  };

  for (const used_variable &used_var : current_scope.variables_used) {
    QLJS_ASSERT(!current_scope.find_declared_variable(used_var.name));
    const declared_variable *var =
        parent_scope.find_declared_variable(used_var.name);
    if (var) {
      // This variable was declared in the parent scope. Don't propagate.
      if (used_var.kind == used_variable_kind::assignment) {
        this->report_error_if_assignment_is_illegal(var, used_var.name);
      }
    } else if (consume_arguments &&
               used_var.name.string_view() == u8"arguments") {
      // Treat this variable as declared in the current scope.
    } else if (is_current_scope_function_name(used_var)) {
      // Treat this variable as declared in the current scope.
    } else {
      (allow_variable_use_before_declaration
           ? parent_scope.variables_used_in_descendant_scope
           : parent_scope.variables_used)
          .emplace_back(used_var);
    }
  }
  current_scope.variables_used.clear();

  for (const used_variable &used_var :
       current_scope.variables_used_in_descendant_scope) {
    const declared_variable *var =
        parent_scope.find_declared_variable(used_var.name);
    if (var) {
      // This variable was declared in the parent scope. Don't propagate.
      if (used_var.kind == used_variable_kind::assignment) {
        this->report_error_if_assignment_is_illegal(var, used_var.name);
      }
    } else if (is_current_scope_function_name(used_var)) {
      // Treat this variable as declared in the current scope.
    } else {
      parent_scope.variables_used_in_descendant_scope.emplace_back(used_var);
    }
  }
  current_scope.variables_used_in_descendant_scope.clear();
}

void linter::propagate_variable_declarations_to_parent_scope() {
  QLJS_ASSERT(this->scopes_.size() >= 2);
  scope &current_scope = this->scopes_[this->scopes_.size() - 1];
  scope &parent_scope = this->scopes_[this->scopes_.size() - 2];

  for (const auto &[_var_name, vars] : current_scope.declared_variables) {
    for (const declared_variable &var : vars) {
      if (var.kind == variable_kind::_function ||
          var.kind == variable_kind::_var) {
        QLJS_ASSERT(var.declaration.has_value());
        this->declare_variable(
            /*scope=*/parent_scope,
            /*name=*/*var.declaration,
            /*kind=*/var.kind,
            /*variable_scope=*/
            declared_variable_scope::declared_in_descendant_scope);
      }
    }
  }
}

void linter::report_error_if_assignment_is_illegal(
    const linter::declared_variable *var, const identifier &assignment) const {
  switch (var->kind) {
    case variable_kind::_const:
    case variable_kind::_import:
      if (var->declaration.has_value()) {
        this->error_reporter_->report_error_assignment_to_const_variable(
            *var->declaration, assignment, var->kind);
      } else {
        this->error_reporter_->report_error_assignment_to_const_global_variable(
            assignment);
      }
      break;
    case variable_kind::_catch:
    case variable_kind::_class:
    case variable_kind::_function:
    case variable_kind::_let:
    case variable_kind::_parameter:
    case variable_kind::_var:
      break;
  }
}

void linter::report_error_if_variable_declaration_conflicts_in_scope(
    const linter::scope &scope, identifier name, variable_kind kind,
    linter::declared_variable_scope declaration_scope) const {
  const declared_variable *already_declared_variable =
      scope.find_declared_variable(name);
  if (already_declared_variable) {
    using vk = variable_kind;
    vk other_kind = already_declared_variable->kind;

    switch (other_kind) {
      case vk::_catch:
        QLJS_ASSERT(kind != vk::_catch);
        QLJS_ASSERT(kind != vk::_import);
        QLJS_ASSERT(kind != vk::_parameter);
        break;
      case vk::_class:
      case vk::_const:
      case vk::_function:
      case vk::_let:
      case vk::_var:
        QLJS_ASSERT(kind != vk::_catch);
        QLJS_ASSERT(kind != vk::_parameter);
        break;
      case vk::_parameter:
        QLJS_ASSERT(kind != vk::_catch);
        QLJS_ASSERT(kind != vk::_import);
        break;
      case vk::_import:
        break;
    }

    bool redeclaration_ok =
        (other_kind == vk::_function && kind == vk::_parameter) ||
        (other_kind == vk::_function && kind == vk::_function) ||
        (other_kind == vk::_parameter && kind == vk::_function) ||
        (other_kind == vk::_var && kind == vk::_function) ||
        (other_kind == vk::_parameter && kind == vk::_parameter) ||
        (other_kind == vk::_catch && kind == vk::_var) ||
        (other_kind == vk::_function && kind == vk::_var) ||
        (other_kind == vk::_parameter && kind == vk::_var) ||
        (other_kind == vk::_var && kind == vk::_var) ||
        (other_kind == vk::_function &&
         already_declared_variable->declaration_scope ==
             declared_variable_scope::declared_in_descendant_scope) ||
        (kind == vk::_function &&
         declaration_scope ==
             declared_variable_scope::declared_in_descendant_scope);
    if (!redeclaration_ok) {
      if (already_declared_variable->declaration.has_value()) {
        this->error_reporter_->report_error_redeclaration_of_variable(
            name, *already_declared_variable->declaration);
      } else {
        this->error_reporter_->report_error_redeclaration_of_global_variable(
            name);
      }
    }
  }
}

const linter::declared_variable *linter::scope::add_variable_declaration(
    identifier name, variable_kind kind,
    declared_variable_scope declared_scope) {
  string8_view name_view = name.string_view();
  return &this->declared_variables[name_view].emplace_back(
      declared_variable{kind, name, declared_scope});
}

void linter::scope::add_predefined_variable_declaration(const char8 *name,
                                                        variable_kind kind) {
  string8_view name_view = name;
  this->declared_variables[name_view].emplace_back(declared_variable{
      .kind = kind,
      .declaration = std::nullopt,
      .declaration_scope = declared_variable_scope::declared_in_current_scope,
  });
}

const linter::declared_variable *linter::scope::find_declared_variable(
    identifier name) const noexcept {
  auto it = this->declared_variables.find(name.string_view());
  if (it == this->declared_variables.end()) {
    return nullptr;
  } else {
    const std::vector<declared_variable> &vars = it->second;
    QLJS_ASSERT(!vars.empty());
    return &vars[0];
  }
}
}
