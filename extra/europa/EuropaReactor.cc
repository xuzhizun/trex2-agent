/*********************************************************************
 * Software License Agreement (BSD License)
 * 
 *  Copyright (c) 2011, MBARI.
 *  All rights reserved.
 * 
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 * 
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above
 *     copyright notice, this list of conditions and the following
 *     disclaimer in the documentation and/or other materials provided
 *     with the distribution.
 *   * Neither the name of the TREX Project nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 * 
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 *  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 *  COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 *  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 *  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 *  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 *  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 *  ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 */
#include "EuropaReactor.hh"
#include "bits/europa_convert.hh"
#include "core/private/CurrentState.hh"

#include <PLASMA/Timeline.hh>
#include <PLASMA/Token.hh>
#include <PLASMA/TokenVariable.hh>

#include <PLASMA/PlanDatabaseWriter.hh>

#include <boost/scope_exit.hpp>

using namespace TREX::europa;
using namespace TREX::transaction;
using namespace TREX::utils;

/*
 * class TREX::europa::EuropaReactor
 */ 

// structors 

EuropaReactor::EuropaReactor(TeleoReactor::xml_arg_type arg)
  :TeleoReactor(arg, false), 
   Assembly(parse_attr<std::string>(xml_factory::node(arg), "name")) {
  bool found;
  std::string nddl;
  
  boost::property_tree::ptree::value_type &cfg = xml_factory::node(arg);
  boost::optional<std::string> 
    model = parse_attr< boost::optional<std::string> >(cfg, "model");
  
  // Load the specified model
  if( model ) {
    if( model->empty() ) 
      throw XmlError(cfg, "Attribute \"model\" is empty.");
    nddl = manager().use(*model, found);
    if( !found )
      throw XmlError(cfg, "Unable to locate model file \""+(*model)+"\"");
  } else {
    std::string short_nddl = getName().str()+".nddl",
      long_nddl = getGraphName().str()+"."+short_nddl;
    
    syslog()<<"No model specified: attempting to load "<<long_nddl;
    nddl = manager().use(long_nddl, found);
    if( !found ) {
      syslog()<<long_nddl<<" not found: attempting to load "<<short_nddl;
      nddl = manager().use(short_nddl, found);
      if( !found )
	throw ReactorException(*this, "Unable to locate "+long_nddl+" or "+short_nddl);
    }
  }
  // Load the nddl model
  if( !playTransaction(nddl) ) 
    throw ReactorException(*this, "model in "+nddl+" is inconsistent.");
    
  if( !plan_db()->isClosed() ) {
    syslog("WARN")<<"Plan database is not closed:\n\tClosing it now!!!";
    plan_db()->close();
  }

  // Loading solver configuration 
  std::string solver_cfg = parse_attr<std::string>(cfg, "solverConfig");
  // boost::optional<std::string> 
  //   synch_cfg = parse_attr< boost::optional<std::string> >(cfg, "synchCfg");
  
  if( solver_cfg.empty() )
    throw XmlError(cfg, "attribute \"solverConfig\" is empty");
  solver_cfg = manager().use(solver_cfg, found);
  
  if( !found ) 
    throw ReactorException(*this, "Unable to locate solver config \""+solver_cfg+"\".");
  
  configure_solvers(solver_cfg);

  // Create reactor connections 
  std::list<EUROPA::ObjectId> objs;
  
  trex_timelines(objs);
  syslog()<<" Found "<<objs.size()<<" TREX "<<TREX_TIMELINE.toString()
	  <<" declarations.";

  for(std::list<EUROPA::ObjectId>::const_iterator o=objs.begin();
      objs.end()!=o; ++o) {
    EUROPA::LabelStr name = (*o)->getName(), mode_val;
    Symbol trex_name(name.c_str());
    EUROPA::ConstrainedVariableId o_mode = mode(*o);

    if( !o_mode->lastDomain().isSingleton() )
      throw ReactorException(*this, "The mode of the "+TREX_TIMELINE.toString()
			     +" \""+trex_name.str()+"\" is not a singleton.");
    else {
      mode_val = o_mode->lastDomain().getSingletonValue();
    }
    
    if( EXTERNAL_MODE==mode_val || OBSERVE_MODE==mode_val ) {
      use(trex_name, OBSERVE_MODE!=mode_val);
      add_state_var(*o);
    } else if( INTERNAL_MODE==mode_val ) {
      provide(trex_name);
      add_state_var(*o);
    } else if( IGNORE_MODE==mode_val ) {
      ignore(*o);
    } else {
      if( PRIVATE_MODE!=mode_val )
	syslog("WARN")<<TREX_TIMELINE.toString()<<" "<<trex_name<<" mode \""
		      <<mode_val.toString()<<"\" is unknown!!!\n"
		      <<"\tI'll assume it is "<<PRIVATE_MODE.toString();
    }      
  }
}

EuropaReactor::~EuropaReactor() {}


// callbacks

//  - TREX transaction callback 

void EuropaReactor::notify(Observation const &obs) {
  setStream();

  EUROPA::ObjectId obj = plan_db()->getObject(obs.object().str());
  bool undefined;
  std::string pred = obs.predicate().str();
  EUROPA::TokenId fact = new_obs(obj, pred, undefined);

  if( undefined ) 
    syslog("WARN")<<"Predicate "<<obs.object()<<"."<<obs.predicate()<<" is unknown"
		  <<"\n\t Created "<<pred<<" instead.";
  else if( !restrict_token(fact, obs) )
    syslog("ERROR")<<"Failed to restrict some attributes of observation "<<obs;
}

void EuropaReactor::handleRequest(goal_id const &request) {
  setStream();

  EUROPA::ObjectId obj = plan_db()->getObject(request->object().str());
  std::string pred = request->predicate().str();
  
  if( !have_predicate(obj, pred) ) {
    syslog("ERROR")<<"Ignoring Unknow token type "<<request->object()<<'.'
		   <<request->predicate();
  } else {
    // Create the new fact
    EUROPA::TokenId goal = create_token(obj, pred, false);

    if( !restrict_token(goal, *request) ) {
      syslog("ERROR")<<"Failed to restrict some attributes of request "<<*request
		     <<"\n\t rejecting it.";
      goal->discard();
    } else {
      // The goal appears to be correct so far : add it to my set of goals
      syslog()<<"Integrated request "<<request<<" as the token with Europa ID "
	      <<goal->getKey();
      m_active_requests.insert(goal_map::value_type(goal, request));
    }
  }
}

void EuropaReactor::handleRecall(goal_id const &request) {
  setStream();
  // Remove the goal if it exists 
  goal_map::right_iterator i = m_active_requests.right.find(request);
  if( m_active_requests.right.end()!=i ) {
    m_active_requests.right.erase(i);
    recalled(i->second);
  }
}

// TREX execution callbacks
void EuropaReactor::handleInit() {
  setStream();
  {
    init_clock_vars();
  }
}

void EuropaReactor::handleTickStart() {
  setStream();  
  // Updating the clock
  clock()->restrictBaseDomain(EUROPA::IntervalIntDomain(now(), final_tick()));
  new_tick();
  if( m_completed_this_tick ) {
    Assembly::external_iterator from(begin(), end()), to(end(), end());
    for(; to!=from; ++from) {
      TeleoReactor::external_iterator 
        j=find_external((*from)->timeline()->getName().c_str());
      EUROPA::eint e_lo, e_hi;
      
      if( j.valid() && j->accept_goals() ) {
        IntegerDomain window = j->dispatch_window(getCurrentTick());
        IntegerDomain::bound lo = window.lowerBound(), hi = window.upperBound();
        e_lo = lo.value();
        if( hi.isInfinity() )
          e_hi = final_tick();
        else
          e_hi = hi.value();
        (*from)->do_dispatch(e_lo, e_hi);
      }
    }
  }
}

bool EuropaReactor::dispatch(EUROPA::TimelineId const &tl, 
                             EUROPA::TokenId const &tok) {
  if( m_dispatched.left.find(tok)==m_dispatched.left.end() ) {
    TREX::utils::Symbol name(tl->getName().toString());
    Goal my_goal(name, tok->getUnqualifiedPredicateName().toString());    
    std::vector<EUROPA::ConstrainedVariableId> const &attrs = tok->parameters();

    // Get start, duration and end
    std::auto_ptr<DomainBase> 
      d_start(details::trex_domain(tok->start()->lastDomain())),
      d_duration(details::trex_domain(tok->duration()->lastDomain())),
      d_end(details::trex_domain(tok->end()->lastDomain()));
    
    my_goal.restrictTime(*dynamic_cast<IntegerDomain *>(d_start.get()), 
                         *dynamic_cast<IntegerDomain *>(d_duration.get()), 
                         *dynamic_cast<IntegerDomain *>(d_end.get()));

    // Manage other attributes
    for(std::vector<EUROPA::ConstrainedVariableId>::const_iterator a=attrs.begin();
        attrs.end()!=a; ++a) {
      std::auto_ptr<DomainBase> dom(details::trex_domain((*a)->lastDomain()));
      Variable attr((*a)->getName().toString(), *dom);
      my_goal.restrictAttribute(attr);
    }
    goal_id request = postGoal(my_goal);
    if( request ) {
      m_dispatched.insert(goal_map::value_type(tok, request));
    } else 
      return false;
  }
  return true;
}

bool EuropaReactor::do_relax(bool full) {
  logPlan("failed");
  bool ret = relax(full);
  logPlan("relax");
  return ret;
}


bool EuropaReactor::synchronize() {
  setStream();
  EuropaReactor &me = *this;

  debugMsg("trex:synch", "["<<now()<<"] BEGIN synchronization =====================================");
  me.logPlan("tick");
  BOOST_SCOPE_EXIT((&me)) {
    me.synchronizer()->clear();    
    me.logPlan("synch");
    debugMsg("trex:synch", "["<<me.now()<<"] END synchronization =======================================");
    debugMsg("trex:synch", "Plan after synchronization:\n"
             <<EUROPA::PlanDatabaseWriter::toString(me.plan_db()));
  } BOOST_SCOPE_EXIT_END

  
  if( !do_synchronize() ) {
    std::string full_name = manager().file_name(getName().str()+".relax.dot");
    m_completed_this_tick = false;
    syslog("WARN")<<"Failed to synchronize : relaxing current plan.";
    
    if( !( do_relax(false) && do_synchronize() ) ) {
      syslog("WARN")<<"Failed to synchronize(2) : forgetting past.";
      if( !( do_relax(true) && do_synchronize() ) ) 
        return false;
    }
  }
  // Prepare the reactor for next deliberation round
  if( m_completed_this_tick ) {
    planner()->clear(); // remove the past decisions of the planner
    
    Assembly::external_iterator from(begin(), end()), to(end(), end()); 
    for( ; to!=from; ++from) {
      EUROPA::TokenId cur = (*from)->current();
      if( cur->isMerged() )
        m_dispatched.left.erase(cur->getActiveToken());
    }
    
    m_completed_this_tick = false;
  }
    
  return constraint_engine()->propagate(); // should not fail 
}

bool EuropaReactor::discard(EUROPA::TokenId const &tok) {
  goal_map::left_iterator i = m_active_requests.left.find(tok);
  
  if( m_active_requests.left.end()!=i ) {
    syslog()<<"Discarded past request ["<<i->second<<"]";
    m_active_requests.left.erase(i);
    return true;
  } 
  return false;
}

void EuropaReactor::cancel(EUROPA::TokenId const &tok) {
  goal_map::left_iterator i = m_dispatched.left.find(tok);
    
  if( m_dispatched.left.end()!=i ) {
    syslog()<<"Recall ["<<i->second<<"]";
    postRecall(i->second);
    m_dispatched.left.erase(i);
  } 
}

bool EuropaReactor::hasWork() {
  setStream();
  if( constraint_engine()->provenInconsistent() ) {
    syslog("ERROR")<<"Plan database is inconsistent.";
    return false;
  }
  if( planner()->isExhausted() ) {
    syslog("WARN")<<"Deliberation solver is exhausted.";
    return false;
  }
  if( !m_completed_this_tick ) {
    if( planner()->noMoreFlaws() ) {
      size_t steps = planner()->getStepCount();
      m_completed_this_tick = true;
      planner()->clear();
      archive();
      if( steps>0 ) {
        syslog()<<"Deliberation completed in "<<steps<<" steps.";
        logPlan("plan");
      }
    }
  }
  return !m_completed_this_tick;
}

void EuropaReactor::resume() {
  setStream();
  
  if( constraint_engine()->pending() )
    constraint_engine()->propagate();
  
  if( constraint_engine()->constraintConsistent() )
    planner()->step();
  
  bool should_relax = false;
  
  if( constraint_engine()->provenInconsistent() ) {
    syslog("WARN")<<"Inconsitency found during planning.";
    should_relax = true;
  }
  if( planner()->isExhausted() ) {
    syslog("WARN")<<"Deliberation solver is exhausted.";
    should_relax = true;
  }
  
  if( should_relax ) {
    syslog("WARN")<<"Relax database after "<<planner()->getStepCount()<<" steps."; 
    if( !relax(false) )
      syslog("WARN")<<"Failed to relax => forgetting past."; 
      if( !relax(true) ) {
        syslog("ERROR")<<"Unable to recover from plan inconsistency.";
	throw TREX::transaction::ReactorException(*this, "Unable to recover from plan inconsistency."); 
      }
  }
}

// europa core callbacks

void EuropaReactor::notify(EUROPA::LabelStr const &object, 
			   EUROPA::TokenId const &tok) {
  Observation obs(object.c_str(), 
		  tok->getUnqualifiedPredicateName().toString());
  
  std::vector<EUROPA::ConstrainedVariableId> const &attr = tok->parameters();

  for(std::vector<EUROPA::ConstrainedVariableId>::const_iterator a=attr.begin();
      attr.end()!=a; ++a) {
    std::auto_ptr<TREX::transaction::DomainBase> 
      dom(details::trex_domain((*a)->lastDomain()));
    TREX::transaction::Variable var((*a)->getName().toString(), *dom);
    obs.restrictAttribute(var);
  }
  postObservation(obs);
}


// manipulators

bool EuropaReactor::restrict_token(EUROPA::TokenId &tok, 
				   Predicate const &pred) {
  bool no_empty = true;
  std::list<Symbol> attrs;
  pred.listAttributes(attrs, false);

  for(std::list<Symbol>::const_iterator v=attrs.begin(); attrs.end()!=v; ++v) {
    EUROPA::ConstrainedVariableId param = tok->getVariable(v->str());
    
    if( param.isId() ) {
      Variable const &var = pred[*v];
      //syslog("INFO")<<"Apply "<<tok->toString()<<"."<<var;
      try {        
	details::europa_restrict(param, var.domain());
      } catch(DomainExcept const &e) {
	syslog("WARN")<<"Failed to restrict attribute "<<(*v)
		      <<" on token "<<pred.object()<<'.'<<pred.predicate()
		      <<": "<<e;
	no_empty = false;
      }
    } else 
      syslog("WARN")<<" Ignoring unknown attribute "<<pred.object()
		    <<'.'<<pred.predicate()<<'.'<<(*v);
  }
  return no_empty;
}

// Observers 

EUROPA::IntervalIntDomain EuropaReactor::plan_scope() const {
  EUROPA::eint scope_duration(getExecLatency()+getLookAhead());
  return EUROPA::IntervalIntDomain(now(), std::min(now()+scope_duration, 
						   final_tick()));
}

void EuropaReactor::logPlan(std::string const &base_name) const {
  std::string full_name = manager().file_name(getName().str()+"."+base_name+".dot");
  std::ofstream out(full_name.c_str());
  print_plan(out);
}


