/**
   \file
   Implementation of calss ProcessPSetBuilder

   \author Stefano ARGIRO
   \version $Id: ProcessPSetBuilder.cc,v 1.12 2006/04/04 22:57:30 rpw Exp $
   \date 17 Jun 2005
*/

static const char CVSId[] = "$Id: ProcessPSetBuilder.cc,v 1.12 2006/04/04 22:57:30 rpw Exp $";


#include <FWCore/ParameterSet/interface/ProcessPSetBuilder.h>
#include "FWCore/ParameterSet/interface/Makers.h"
#include "FWCore/ParameterSet/interface/parse.h"
#include "FWCore/ParameterSet/interface/Nodes.h"
#include "FWCore/ParameterSet/interface/ParameterSet.h"
#include "FWCore/ParameterSet/interface/ProcessDesc.h"
#include "FWCore/ParameterSet/interface/Entry.h"

#include "FWCore/ParameterSet/src/ScheduleValidator.h"
#include "FWCore/Utilities/interface/EDMException.h"
#include "FWCore/Utilities/interface/DebugMacros.h"

#include "boost/shared_ptr.hpp"
#include <vector>
#include <map>
#include <stdexcept>
#include <iostream>
#include <algorithm>
#include <iterator>

using namespace boost;
using namespace std;

namespace edm
{

  ProcessPSetBuilder::~ProcessPSetBuilder()
  {
    delete validator_;
  }

  ProcessPSetBuilder::ProcessPSetBuilder(const std::string& config)
  {
    edm::pset::ParseResults parsetree = edm::pset::fullParse(config.c_str());

    processDesc_= edm::pset::makeProcess(parsetree);

    SeqMap sequences;

    // loop on path fragments
    ProcessDesc::PathContainer::iterator pathIt; 
    Strs endpaths, triggerpaths;
   
    for(pathIt= processDesc_->pathFragments_.begin();
	pathIt!=processDesc_->pathFragments_.end();
	++pathIt){
     
      if ((*pathIt)->type()=="sequence") {
	sequences[(*pathIt)->name]= (*pathIt);
      }
     
      if ((*pathIt)->type()=="path") {
	sequenceSubstitution((*pathIt)->wrapped_, sequences);
	//fillPath((*pathIt),pathnames,&processDesc_->pset_);
	fillPath((*pathIt),triggerpaths,&processDesc_->pset_);
      }

      if ((*pathIt)->type()=="endpath") {
	//cout << "got endpath = " << (*pathIt)->name << endl;
	//cout << "pointer = " << typeid(*(*pathIt)->wrapped_.get()).name << endl;
	sequenceSubstitution((*pathIt)->wrapped_, sequences);
	//fillPath((*pathIt),pathnames,&processDesc_->pset_);
	fillPath((*pathIt),endpaths,&processDesc_->pset_);
      }
     
     
    } // loop on path fragments
    
    Strs pathnames(triggerpaths);
    pathnames.insert(pathnames.end(),endpaths.begin(),endpaths.end());

    if(1 <= edm::debugit())
      {
	std::cerr << "\npathnames=\n  ";
	std::copy(pathnames.begin(),pathnames.end(),
		  std::ostream_iterator<std::string>(std::cerr,","));
	std::cerr << "\ntriggernames=\n  ";
	std::copy(triggerpaths.begin(),triggerpaths.end(),
		  std::ostream_iterator<std::string>(std::cerr,","));
	std::cerr << "\nendpaths=\n  ";
	std::copy(endpaths.begin(),endpaths.end(),
		  std::ostream_iterator<std::string>(std::cerr,","));
	std::cerr << "\n";
      }

    ParameterSet paths_trig;
    paths_trig.insert(true,"@paths",Entry(triggerpaths,true));
    paths_trig.insert(true,"@end_paths",Entry(endpaths,true));

    processDesc_->pset_.insert(true,"@trigger_paths",Entry(paths_trig,false));
    processDesc_->pset_.insert(true,"@paths",Entry(pathnames,true));
   
    validator_= 
      new ScheduleValidator(processDesc_->pathFragments_,processDesc_->pset_); 
   
    validator_->validate();
   
    processPSet_= 
      shared_ptr<edm::ParameterSet>(new ParameterSet(processDesc_->pset_));
    servicePSets_=
      shared_ptr<std::vector<edm::ParameterSet> >(
						  new std::vector<edm::ParameterSet>(processDesc_->services_));
  }


  void 
  ProcessPSetBuilder::getNames(const edm::pset::Node* n, Strs& out){
    if(n->type()=="operand"){ 
      out.push_back(n->name);
    } else {	
      const edm::pset::OperatorNode& op = dynamic_cast<const edm::pset::OperatorNode&>(*n);
      getNames(op.left_.get(),out);
      getNames(op.right_.get(),out);
    }
  } // getNames


  void ProcessPSetBuilder::fillPath(WrapperNodePtr n, Strs&   paths,  
				    ParameterSet* out){
  
    Strs names;
    getNames(n->wrapped_.get(),names);    
    out->insert(true,n->name,Entry(names,true));
    paths.push_back(n->name); // add to the list of paths
  
  } // fillPath(..) 



  void ProcessPSetBuilder::sequenceSubstitution(NodePtr& node, 
						SeqMap&  sequences){
  
    if (node->type()=="operand"){
      SeqMap::iterator seqIt = sequences.find(node->name); 
      if (seqIt!= sequences.end()){
	node = seqIt->second->wrapped_;
      }
    } // if operator
    else {
      edm::pset::OperatorNode* onode = dynamic_cast<edm::pset::OperatorNode*>(node.get());
    
    
      SeqMap::iterator seqIt = sequences.find(onode->left_->name); 
      if (seqIt!= sequences.end()) {
	onode->left_= seqIt->second->wrapped_;
	onode->left_->setParent(onode);
      }
      seqIt = sequences.find(onode->right_->name); 
      if (seqIt!= sequences.end()){
	onode->right_= seqIt->second->wrapped_; 
	onode->right_->setParent(onode);
      }
      sequenceSubstitution(onode->left_, sequences);
      sequenceSubstitution(onode->right_,sequences);
    
    }// else (operand)
  
  } // sequenceSubstitution


  void ProcessPSetBuilder::dumpTree(NodePtr& node){
    if(node->type()=="operand"){ 
      cout << " Operand " << node->name<< " p:";
      if (node->getParent()) cout <<  node->getParent()->name;cout<< endl;
    } else{	
      edm::pset::OperatorNode* op = dynamic_cast<edm::pset::OperatorNode*>(node.get());
      cout << " Operator: " << op->name<<"["<<op->type()<<"]" 
	   << " l:" << op->left_ << " r:"<<op->right_<< " p:";
      if (op->parent_)cout<<  op->parent_->name;cout<< endl;
      dumpTree(op->left_);
      dumpTree(op->right_);
    }
  } // dumpTree


  std::string  ProcessPSetBuilder::getDependencies(const std::string& modulename){
    return validator_->dependencies(modulename);
  }


  boost::shared_ptr<edm::ParameterSet>  
  ProcessPSetBuilder::getProcessPSet() const{

    return processPSet_;

  }

  boost::shared_ptr<std::vector<ParameterSet> > 
  ProcessPSetBuilder::getServicesPSets() const{
    return servicePSets_;
  }
} // namespace edm
