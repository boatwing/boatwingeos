#include "./boatwingio.hpp"

void token::create( const name&   issuer,
                    const asset&  maximum_supply )
{
   require_auth( get_self() );

   check( is_account( issuer ), "issuer account does not exist" );

   auto sym = maximum_supply.symbol;

   check( sym.is_valid(), "invalid symbol name" );
   check( maximum_supply.is_valid(), "invalid supply");
   check( maximum_supply.amount > 0, "max-supply must be positive");

   stats statstable( get_self(), sym.code().raw() );
   auto existing = statstable.find( sym.code().raw() );

   check( existing == statstable.end(), "token with symbol already exists" );

   statstable.emplace( get_self(), [&]( auto& s ) {
      s.supply.symbol = maximum_supply.symbol;
      s.max_supply    = maximum_supply;
      s.issuer        = issuer;
      s.refund_delay = 0;
      s.fee_ratio = 0;
      s.fee_receiver = issuer;
   });

   staketotal totaltable( get_self(), get_first_receiver().value );
   auto total_itr = totaltable.find( sym.code().raw() );

   check( total_itr == totaltable.end() , "token with symbol already exists" );

   totaltable.emplace(get_self(), [&]( auto& r) {
      r.staked_balance_total = asset{ 0, sym };
   });   
}

void token::setdelay(const symbol& symbol, uint64_t delaytime) {
   auto sym_code_raw = symbol.code().raw();

   stats statstable( get_self(), sym_code_raw );
   auto& st = statstable.get( sym_code_raw, "symbol does not exist" );
   
   require_auth( st.issuer );
   check( st.supply.symbol.code() == symbol.code(), "symbol precision mismatch" );

   statstable.modify( st, same_payer, [&]( auto& s ) {
      s.refund_delay = delaytime;
   });
}

void token::settransfee(const symbol& symbol, uint64_t ratio, name receiver) {
   auto sym_code_raw = symbol.code().raw();

   check( is_account( receiver ), "receiver account does not exist" );

   stats statstable( get_self(), sym_code_raw );
   auto& st = statstable.get( sym_code_raw, "symbol does not exist" );

   require_auth( st.issuer );
   check( st.supply.symbol.code() == symbol.code(), "symbol precision mismatch ");
   check( ratio >= 0 && ratio <= 100, " transfer fee is out of boundary");
   
   statstable.modify( st, same_payer, [&]( auto& s ) {
      s.fee_ratio = ratio;
      s.fee_receiver = receiver;
   });
}

void token::issue( const name& to, const asset& quantity, const string& memo )
{
   check( is_account( to ), "to account does not exist" );

   auto sym = quantity.symbol;
   check( sym.is_valid(), "invalid symbol name" );
   check( memo.size() <= 256, "memo has more than 256 bytes" );

   stats statstable( get_self(), sym.code().raw() );
   auto existing = statstable.find( sym.code().raw() );
   check( existing != statstable.end(), "token with symbol does not exist, create token before issue" );
   const auto& st = *existing;

   require_auth( st.issuer );
   check( quantity.is_valid(), "invalid quantity" );
   check( quantity.amount > 0, "must issue positive quantity" );

   check( quantity.symbol == st.supply.symbol, "symbol precision mismatch" );
   check( quantity.amount <= st.max_supply.amount - st.supply.amount, "quantity exceeds available supply");

   statstable.modify( st, same_payer, [&]( auto& s ) {
      s.supply += quantity;
   });

   add_balance( st.issuer, quantity, st.issuer );
}

void token::retire( const asset& quantity, const string& memo )
{
   auto sym = quantity.symbol;
   check( sym.is_valid(), "invalid symbol name" );
   check( memo.size() <= 256, "memo has more than 256 bytes" );

   stats statstable( get_self(), sym.code().raw() );
   auto existing = statstable.find( sym.code().raw() );
   check( existing != statstable.end(), "token with symbol does not exist" );
   const auto& st = *existing;

   require_auth( st.issuer );
   check( quantity.is_valid(), "invalid quantity" );
   check( quantity.amount > 0, "must retire positive quantity" );

   check( quantity.symbol == st.supply.symbol, "symbol precision mismatch" );

   statstable.modify( st, same_payer, [&]( auto& s ) {
      s.supply -= quantity;
   });

   sub_balance( st.issuer, quantity );
}

void token::transfer(const name&    from,
                     const name&    to,
                     const asset&   quantity,
                     const string&  memo )
{
   check( from != to, "cannot transfer to self" );
   require_auth( from );
   check( is_account( to ), "to account does not exist");
   auto sym = quantity.symbol.code();
   stats statstable( get_self(), sym.raw() );
   const auto& st = statstable.get( sym.raw() );

   require_recipient( from );
   require_recipient( to );

   check( quantity.is_valid(), "invalid quantity" );
   check( quantity.amount > 0, "must transfer positive quantity" );
   check( quantity.symbol == st.supply.symbol, "symbol precision mismatch" );
   check( memo.size() <= 256, "memo has more than 256 bytes" );

   auto payer = has_auth( to ) ? to : from;

   sub_balance( from, quantity );
   add_balance( to, quantity, payer );
}

void token::stake(name owner, asset quantity) {
   require_auth(owner);

   check( quantity.is_valid(), "invalid quantity");
   check( quantity.amount > 0, "must stake positive quantity");

   const auto& sym_code_raw = quantity.symbol.code().raw();

   accounts from_acnts(get_self(), owner.value);
   const auto& from = from_acnts.get(sym_code_raw, "no balance object found");

   check(from.balance >= (from.staked_balance + quantity), "overdrawn balance for stake action");

   from_acnts.modify(from, owner, [&]( auto& a ) {
      a.staked_balance += quantity;
   });

   stakestats stakestable( get_self(), sym_code_raw);
   auto userstake = stakestable.find(owner.value);

   if(userstake == stakestable.end()) {
      stakestable.emplace(owner, [&]( auto& r ) {
         r.owner = owner;
         r.staked_balance = quantity;
      });
   } else {
      stakestable.modify(userstake, same_payer, [&]( auto& r ) {
         r.staked_balance += quantity;
      });
   }

   staketotal totaltable( get_self(), get_first_receiver().value );
   auto total_itr = totaltable.find(sym_code_raw);

   check( total_itr != totaltable.end(), "token object does not exist ");

   totaltable.modify(total_itr, get_self(), [&]( auto& r ) {
      r.staked_balance_total += quantity;
   });
}

void token::unstake(name owner, asset quantity) {
   require_auth(owner);

   check(quantity.is_valid(), "invalid quantity");
   check(quantity.amount > 0, "must unstake positive quantity");

   const auto& sym_code_raw = quantity.symbol.code().raw();

   stats statstable( get_self(), sym_code_raw );
   const auto& st = statstable.get(sym_code_raw, "symbol does not exist");

   accounts from_acnts(get_self(), owner.value);
   const auto& from = from_acnts.get(sym_code_raw, "no balance object found");

   check(from.staked_balance >= quantity, "overdrawn staked balance");

   unstakestats unstaketable( get_self(), sym_code_raw );
   auto itr = unstaketable.find(owner.value);

   check(itr == unstaketable.end(), "refunding request already exist");

   auto current_time = current_time_point().sec_since_epoch();
   unstaketable.emplace(owner, [&]( auto& r ) {
      r.owner = owner;
      r.request_time = current_time;
      r.refund_time = current_time + st.refund_delay;
      r.amount = quantity;
   });
}

void token::refund(name owner, symbol_code& symbol) {
   require_auth(owner);
   inline_refund(owner, same_payer, symbol);
}

void token::inline_refund(name owner, name rampayer, symbol_code& symbol) {
   auto sym_code_raw = symbol.raw();
   unstakestats unstaketable( get_self(), sym_code_raw );
   auto itr = unstaketable.find(owner.value);

   check( itr != unstaketable.end(), "refund request not found");
   check( itr -> owner == owner, "sender is not matched with owner");
   check( itr -> refund_time <= current_time_point().sec_since_epoch(), "refund is not available yet");

   asset quantity = itr -> amount;

   // Modify Account Balance
   accounts acnt_tbl( get_self(), owner.value );
   auto& from_acnt = acnt_tbl.get( sym_code_raw, "no balance object found");
   
   check( from_acnt.balance >= quantity, "overdrawn staked balance");

   acnt_tbl.modify(from_acnt, rampayer, [&]( auto& a ) {
      a.staked_balance -= quantity;
   });

   // Modify Stake Stats
   stakestats staketable( get_self(), sym_code_raw );
   auto userstake = staketable.find(owner.value);

   check(userstake != staketable.end(), "user not found");

   staketable.modify(userstake, owner, [&]( auto& r ) {
      r.owner = owner;
      r.staked_balance -= quantity;
   });

   // Modify Total Stake
   staketotal totaltable( get_self(), get_first_receiver().value );
   auto total_itr = totaltable.find(sym_code_raw);

   check(total_itr != totaltable.end(), "symbol not found");
   
   totaltable.modify(total_itr, get_self(), [&]( auto& r ) {
      r.staked_balance_total -= quantity;
   });

   unstaketable.erase(itr);
}

void token::cancelrefund(name owner, symbol_code& symbol) {
   require_auth( owner );

   uint128_t sender_id = owner.value;
   cancel_deferred( sender_id );

   auto sym_code_raw = symbol.raw();
   unstakestats unstaketable( get_self(), sym_code_raw );
   auto itr = unstaketable.find( owner.value );
   
   check( itr != unstaketable.end(), "refund request not found");

   unstaketable.erase(itr);
}

void token::sub_balance( const name& owner, const asset& value ) {
   accounts from_acnts( get_self(), owner.value );
   const auto& from = from_acnts.get( value.symbol.code().raw(), "no balance object found" );

   check( from.balance.amount >= value.amount + from.staked_balance.amount, "overdrawn balance" );

   from_acnts.modify( from, owner, [&]( auto& a ) {
         a.balance -= value;
   });
}

void token::add_balance( const name& owner, const asset& value, const name& ram_payer )
{
   accounts to_acnts( get_self(), owner.value );
   auto to = to_acnts.find( value.symbol.code().raw() );
   if( to == to_acnts.end() ) {
      to_acnts.emplace( ram_payer, [&]( auto& a ){
         a.balance = value;
         a.staked_balance = asset { 0, value.symbol };
      });

   } else {
      to_acnts.modify( to, same_payer, [&]( auto& a ) {
         a.balance += value;
      });
   }
}

void token::open( const name& owner, const symbol& symbol, const name& ram_payer )
{
   require_auth( ram_payer );

   check( is_account( owner ), "owner account does not exist" );

   auto sym_code_raw = symbol.code().raw();
   stats statstable( get_self(), sym_code_raw );
   const auto& st = statstable.get( sym_code_raw, "symbol does not exist" );
   check( st.supply.symbol == symbol, "symbol precision mismatch" );

   accounts acnts( get_self(), owner.value );
   auto it = acnts.find( sym_code_raw );
   if( it == acnts.end() ) {
      acnts.emplace( ram_payer, [&]( auto& a ){
        a.balance = asset{0, symbol};
        a.staked_balance = asset{0, symbol};
      });
   }

   stakestats stakestable( get_self(), sym_code_raw );
   auto userstake = stakestable.find(owner.value);

   if(userstake == stakestable.end()) {
      stakestable.emplace( ram_payer, [&]( auto& r ) {
         r.owner = owner;
         r.staked_balance = asset{0, symbol};
      });
   }
}

void token::close( const name& owner, const symbol& symbol )
{
   require_auth( owner );

   auto sym_code_raw = symbol.code().raw();

   accounts acnts( get_self(), owner.value );
   auto it = acnts.find( sym_code_raw );

   check( it != acnts.end(), "Balance row already deleted or never existed. Action won't have any effect." );
   check( it->balance.amount == 0 && it->staked_balance.amount == 0, "ACCOUNTS:: Cannot close because the balance is not zero." );

   acnts.erase( it );

   stakestats stakestable( get_self(), sym_code_raw );
   auto userstake = stakestable.find(owner.value);

   if(userstake != stakestable.end()) {
      check( it -> staked_balance.amount == 0, "STAKESTATS:: Cannot close because the staked_balance is not zero." );

      stakestable.erase(userstake);
   }
}

EOSIO_DISPATCH(token, (create)(setdelay)(settransfee)(issue)(transfer)(stake)(unstake)(refund)(cancelrefund)(open)(close)(retire))