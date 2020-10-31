/** Implementation of the pqxx::transaction class.
 *
 * pqxx::transaction represents a regular database transaction.
 *
 * Copyright (c) 2000-2020, Jeroen T. Vermeulen.
 *
 * See COPYING for copyright license.  If you did not receive a file called
 * COPYING with this source code, please notify the distributor of this
 * mistake, or contact the author.
 */
#include "pqxx-source.hxx"

#include <stdexcept>

#include "pqxx/connection"
#include "pqxx/result"
#include "pqxx/transaction"


pqxx::internal::basic_transaction::basic_transaction(
  connection &c, char const begin_command[], std::string_view tname) :
        dbtransaction(c, tname)
{
  register_transaction();
  direct_exec(begin_command);
}


pqxx::internal::basic_transaction::basic_transaction(
  connection &c, char const begin_command[], std::string &&tname) :
        dbtransaction(c, std::move(tname))
{
  register_transaction();
  direct_exec(begin_command);
}


pqxx::internal::basic_transaction::basic_transaction(
  connection &c, char const begin_command[]) :
        dbtransaction(c)
{
  register_transaction();
  direct_exec(begin_command);
}


void pqxx::internal::basic_transaction::do_commit()
{
  static auto const commit_q{std::make_shared<std::string>("COMMIT"sv)};
  try
  {
    direct_exec(commit_q);
  }
  catch (statement_completion_unknown const &e)
  {
    // Outcome of "commit" is unknown.  This is a disaster: we don't know the
    // resulting state of the database.
    process_notice(internal::concat(e.what(), "\n"));

    std::string msg{internal::concat(
      "WARNING: Commit of transaction '", name(),
      "' is unknown. "
      "There is no way to tell whether the transaction succeeded "
      "or was aborted except to check manually.\n")};
    process_notice(msg);
    // Strip newline.  It was only needed for process_notice().
    msg.pop_back();
    throw in_doubt_error{std::move(msg)};
  }
  catch (std::exception const &e)
  {
    if (not conn().is_open())
    {
      // We've lost the connection while committing.  There is just no way of
      // telling what happened on the other end.  >8-O
      process_notice(internal::concat(e.what(), "\n"));

      auto msg{internal::concat(
        "WARNING: Connection lost while committing transaction '", name(),
        "'. There is no way to tell whether the transaction succeeded "
        "or was aborted except to check manually.\n")};
      process_notice(msg);
      // Strip newline.  It was only needed for process_notice().
      msg.pop_back();
      throw in_doubt_error{std::move(msg)};
    }
    else
    {
      // Commit failed--probably due to a constraint violation or something
      // similar.
      throw;
    }
  }
}


void pqxx::internal::basic_transaction::do_abort()
{
  static auto const rollback_q{std::make_shared<std::string>("ROLLBACK")};
  direct_exec(rollback_q);
}
