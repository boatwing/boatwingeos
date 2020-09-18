#pragma once

#include <eosio/asset.hpp>
#include <eosio/eosio.hpp>
#include <eosio/transaction.hpp>
#include <eosio/time.hpp>

#include <string>

namespace eosiosystem {
   class system_contract;
}

using namespace eosio;
using std::string;


/**
 * @defgroup eosiotoken eosio.token
 * @ingroup eosiocontracts
 *
 * eosio.token contract
 *
 * @details eosio.token contract defines the structures and actions that allow users to create, issue, and manage
 * tokens on eosio based blockchains.
 * @{
 */
class [[eosio::contract("boatwingio")]] token : public contract {
   public:
      using contract::contract;

      /**
       * Create action.
       *
       * @details Allows `issuer` account to create a token in supply of `maximum_supply`.
       * @param issuer - the account that creates the token,
       * @param maximum_supply - the maximum supply set for the token created.
       *
       * @pre Token symbol has to be valid,
       * @pre Token symbol must not be already created,
       * @pre maximum_supply has to be smaller than the maximum supply allowed by the system: 1^62 - 1.
       * @pre Maximum supply must be positive;
       *
       * If validation is successful a new entry in statstable for token symbol scope gets created.
       */
      [[eosio::action]]
      void create( const name&   issuer,
                     const asset&  maximum_supply);

      [[eosio::action]]
      void setdelay(const symbol& symbol, uint64_t delaytime);

      [[eosio::action]]
      void settransfee(const symbol& symbol, uint64_t ratio, name receiver);
      
      /**
       * Issue action.
       *
       * @details This action issues to `to` account a `quantity` of tokens.
       *
       * @param to - the account to issue tokens to, it must be the same as the issuer,
       * @param quntity - the amount of tokens to be issued,
       * @memo - the memo string that accompanies the token issue transaction.
       */
      [[eosio::action]]
      void issue( const name& to, const asset& quantity, const string& memo );

      /**
       * Retire action.
       *
       * @details The opposite for create action, if all validations succeed,
       * it debits the statstable.supply amount.
       *
       * @param quantity - the quantity of tokens to retire,
       * @param memo - the memo string to accompany the transaction.
       */
      [[eosio::action]]
      void retire( const asset& quantity, const string& memo );

      /**
       * Transfer action.
       *
       * @details Allows `from` account to transfer to `to` account the `quantity` tokens.
       * One account is debited and the other is credited with quantity tokens.
       *
       * @param from - the account to transfer from,
       * @param to - the account to be transferred to,
       * @param quantity - the quantity of tokens to be transferred,
       * @param memo - the memo string to accompany the transaction.
       */
      [[eosio::action]]
      void transfer( const name&    from,
                     const name&    to,
                     const asset&   quantity,
                     const string&  memo );

      /**
       * Stake action.
       * 
       * @details User stakes their tokens.
       * 
       * @param owner - the account to stake,
       * @param quantity - the quantity of tokens to be staked.
       */
      [[eosio::action]]
      void stake(name owner, asset quantity);

      /**
       * Unstake action
       * 
       * @details User unstakes theis tokens.
       * 
       * @param owner - the account to unstake,
       * @param quantity - the quantity of tokens to be unstaked.
       */
      [[eosio::action]]
      void unstake(name owner, asset quantity);
   
      /**
       * Refund action
       * 
       * @details Tokens that are queued on the refund table will be returned to user.
       *          Refund is triggered from unstake action.
       * @param owner - the account to get tokens refunded,
       * @param index - index of refund request.
       * 
       * @pre There should be refund requests in the refund data table.
       * @pre Index should be designated by user.
       */
      [[eosio::action]]
      void refund(name owner, symbol_code& symbol);
   
      /**
       * Cancelrefund action
       * 
       * @details It cancels the queued refund request.
       * 
       * @param owner - the account to get tokens refunded,
       * @param index - index of refund request.
       */
      [[eosio::action]]
      void cancelrefund(name owner, symbol_code& symbol);

      /**
       * Open action.
       *
       * @details Allows `ram_payer` to create an account `owner` with zero balance for
       * token `symbol` at the expense of `ram_payer`.
       *
       * @param owner - the account to be created,
       * @param symbol - the token to be payed with by `ram_payer`,
       * @param ram_payer - the account that supports the cost of this action.
       *
       * More information can be read [here](https://github.com/EOSIO/eosio.contracts/issues/62)
       * and [here](https://github.com/EOSIO/eosio.contracts/issues/61).
       */
      [[eosio::action]]
      void open( const name& owner, const symbol& symbol, const name& ram_payer );

      /**
       * Close action.
       *
       * @details This action is the opposite for open, it closes the account `owner`
       * for token `symbol`.
       *
       * @param owner - the owner account to execute the close action for,
       * @param symbol - the symbol of the token to execute the close action for.
       *
       * @pre The pair of owner plus symbol has to exist otherwise no action is executed,
       * @pre If the pair of owner plus symbol exists, the balance has to be zero.
       */
      [[eosio::action]]
      void close( const name& owner, const symbol& symbol );

      /**
       * Get supply method.
       *
       * @details Gets the supply for token `sym_code`, created by `token_contract_account` account.
       *
       * @param token_contract_account - the account to get the supply for,
       * @param sym_code - the symbol to get the supply for.
       */
      static asset get_supply( const name& token_contract_account, const symbol_code& sym_code )
      {
         stats statstable( token_contract_account, sym_code.raw() );
         const auto& st = statstable.get( sym_code.raw() );
         return st.supply;
      }

      /**
       * Get balance method.
       *
       * @details Get the balance for a token `sym_code` created by `token_contract_account` account,
       * for account `owner`.
       *
       * @param token_contract_account - the token creator account,
       * @param owner - the account for which the token balance is returned,
       * @param sym_code - the token for which the balance is returned.
       */
      static asset get_balance( const name& token_contract_account, const name& owner, const symbol_code& sym_code )
      {
         accounts accountstable( token_contract_account, owner.value );
         const auto& ac = accountstable.get( sym_code.raw() );
         return ac.balance;
      }

      using create_action = eosio::action_wrapper<"create"_n, &token::create>;
      using issue_action = eosio::action_wrapper<"issue"_n, &token::issue>;
      using retire_action = eosio::action_wrapper<"retire"_n, &token::retire>;
      using transfer_action = eosio::action_wrapper<"transfer"_n, &token::transfer>;
      using open_action = eosio::action_wrapper<"open"_n, &token::open>;
      using close_action = eosio::action_wrapper<"close"_n, &token::close>;
      using setdelay_action = eosio::action_wrapper<"setdelay"_n, &token::setdelay>;
      using settransfee_action = eosio::action_wrapper<"settransfee"_n, &token::settransfee>;
      using stake_action = eosio::action_wrapper<"stake"_n, &token::stake>;
      using unstake_action = eosio::action_wrapper<"unstake"_n, &token::unstake>;
      using refund_action = eosio::action_wrapper<"refund"_n, &token::refund>;
      using cancelrefund_action = eosio::action_wrapper<"cancelrefund"_n, &token::cancelrefund>;

      // ... END OF PUBLIC

   private:
      struct [[eosio::table]] account {
         asset    balance;
         asset staked_balance;

         uint64_t primary_key()const { return balance.symbol.code().raw(); }
      };

      struct [[eosio::table]] currency_stats {
         asset    supply;
         asset    max_supply;
         name     issuer;
         uint64_t refund_delay;
         uint64_t fee_ratio;
         name fee_receiver;

         uint64_t primary_key()const { return supply.symbol.code().raw(); }
      };

      struct [[eosio::table]] stake_stats {
         name owner;
         asset staked_balance;

         uint64_t primary_key() const { return owner.value; }
      };

      struct [[eosio::table]] stake_total {
         asset staked_balance_total;

         uint64_t primary_key() const { return staked_balance_total.symbol.code().raw(); }
      };

      struct [[eosio::table]] unstake_stats {
         // uint64_t index;
         name owner;
         uint64_t request_time;
         uint64_t refund_time;
         asset amount;

         // uint64_t primary_key() const { return index; }
         // uint64_t secondary_key() const { return owner.value; }
         uint64_t primary_key() const { return owner.value; }
      };

      typedef eosio::multi_index< "accounts"_n, account > accounts;
      typedef eosio::multi_index< "stat"_n, currency_stats > stats;
      typedef eosio::multi_index< "stakestats"_n, stake_stats > stakestats;
      typedef eosio::multi_index< "totalstake"_n, stake_total > staketotal;
      typedef eosio::multi_index< "unstakestats"_n, unstake_stats > unstakestats;

      void sub_balance( const name& owner, const asset& value );
      void add_balance( const name& owner, const asset& value, const name& ram_payer );
      void inline_refund( name owner, name rampayer, symbol_code& symbol);
      asset collect_refund( name owner, const symbol& symbol );
};
/** @}*/ // end of @defgroup eosiotoken eosio.token
