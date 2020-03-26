/*
 * JOOS is Copyright (C) 1997 Laurie Hendren & Michael I. Schwartzbach
 *
 * Reproduction of all or part of this software is permitted for
 * educational or research use on condition that this copyright notice is
 * included in any copy. This software comes with no warranty of any
 * kind. In no event will the authors be liable for any damages resulting from
 * use of this software.
 *
 * email: hendren@cs.mcgill.ca, mis@brics.dk
 */

/* iload x        iload x        iload x
 * ldc 0          ldc 1          ldc 2
 * imul           imul           imul
 * ------>        ------>        ------>
 * ldc 0          iload x        iload x
 *                               dup
 *                               iadd
 *
 * Explanation: Provided with the base
 */

int simplify_multiplication_right(CODE **c)
{ int x,k;
  if (is_iload(*c,&x) && 
      is_ldc_int(next(*c),&k) && 
      is_imul(next(next(*c)))) {
     if (k==0) return replace(c,3,makeCODEldc_int(0,NULL));
     else if (k==1) return replace(c,3,makeCODEiload(x,NULL));
     else if (k==2) return replace(c,3,makeCODEiload(x,
                                       makeCODEdup(
                                       makeCODEiadd(NULL))));
     return 0;
  }
  return 0;
}

/* dup
 * astore x
 * pop
 * -------->
 * astore x
 *
 * Explanation: Provided with the base code
 */
int simplify_astore(CODE **c)
{ int x;
  if (is_dup(*c) &&
      is_astore(next(*c),&x) &&
      is_pop(next(next(*c)))) {
     return replace(c,3,makeCODEastore(x,NULL));
  }
  return 0;
}

/* astore x  
 * aload x
 * -------->
 * dup
 * astore x
 *
 * Explanation: Tries to save 1 byte by using dup instead of aload. 
 * At the end, we have the same on both stacks.
 */
int simplify_astore_aload(CODE **c)
{ int x, y;
  if (is_astore(*c, &x) &&
      is_aload(next(*c), &y) &&
      x == y) {
     return replace(c,2,makeCODEdup(
                        makeCODEastore(x,NULL)));
  }
  return 0;
}

/* iload x
 * ldc k   (0<=k<=127)
 * iadd
 * istore x
 * --------->
 * iinc x k
 *
 * Explanation: Provided with the base
 */ 
int positive_increment(CODE **c)
{ int x,y,k;
  if (is_iload(*c,&x) &&
      is_ldc_int(next(*c),&k) &&
      is_iadd(next(next(*c))) &&
      is_istore(next(next(next(*c))),&y) &&
      x==y && 0<=k && k<=127) {
     return replace(c,4,makeCODEiinc(x,k,NULL));
  }
  return 0;
}

/* goto L1
 * ...
 * L1:
 * goto L2
 * ...
 * L2:
 * --------->
 * goto L2
 * ...
 * L1:    (reference count reduced by 1)
 * goto L2
 * ...
 * L2:    (reference count increased by 1)  
 *
 * Explanation: Provided with the base
 */
int simplify_goto_goto(CODE **c)
{ int l1,l2;
  if (is_goto(*c,&l1) && is_goto(next(destination(l1)),&l2) && l1>l2) {
     droplabel(l1);
     copylabel(l2);
     return replace(c,1,makeCODEgoto(l2,NULL));
  }
  return 0;
}

/*
* if_(i|a)cmp<cond> true_#
* iconst_0
* goto stop_##
* true_#:     // check for unique label
* iconst_1
* stop_##:   // check for unique label
* if(eq|ne) stop_###
* ------------------->
* if_(i|a)cmp<[!]cond> stop_###
*
* Explanation: This follows the pattern for if statements shown in class with more cases. 
* At the end, the stack should be empty (from the point of vue of this chunk of code).
* Depending on the last if's condition, we either flip the first condition or keep
* it the same
*/
int simplify_if_stmt(CODE **c)
{
  int true_label = 0, stop_label = 0, stop_label2 = 0;
  int temp = 0;
  CODE* ptr = *c;
  CODE* replacement = NULL;

  if(
      is_if(c, &true_label) && uniquelabel(true_label) &&
      is_ldc_int((ptr = next(ptr)), &temp) && (temp == 0) &&
      is_goto((ptr = next(ptr)), &stop_label) && uniquelabel(stop_label) &&
      is_label((ptr = next(ptr)), &temp) && (temp == true_label) &&
      is_ldc_int((ptr = next(ptr)), &temp) && (temp == 1) &&
      is_label((ptr = next(ptr)), &temp) && (temp == stop_label) &&
      (is_ifeq((ptr = next(ptr)), &stop_label2) || is_ifne(ptr, &stop_label2)) 
    )
  {
    /*Depending on the last if's condition, we either flip the first condition or keep
      it the same*/
    if (ptr->kind == ifeqCK){
      switch((*c)->kind)
      {
        case ifeqCK:      replacement = makeCODEifne(stop_label2, NULL);
                          break;
        case ifneCK:      replacement = makeCODEifeq(stop_label2, NULL);
                          break;
        case if_acmpeqCK: replacement = makeCODEif_acmpne(stop_label2, NULL);
                          break;
        case if_acmpneCK: replacement = makeCODEif_acmpeq(stop_label2, NULL);
                          break;
        case ifnullCK:    replacement = makeCODEifnonnull(stop_label2, NULL);
                          break;
        case ifnonnullCK: replacement = makeCODEifnull(stop_label2, NULL);
                          break;
        case if_icmpeqCK: replacement = makeCODEif_icmpne(stop_label2, NULL);
                          break;
        case if_icmpgtCK: replacement = makeCODEif_icmple(stop_label2, NULL);
                          break;
        case if_icmpltCK: replacement = makeCODEif_icmpge(stop_label2, NULL);
                          break;
        case if_icmpleCK: replacement = makeCODEif_icmpgt(stop_label2, NULL);
                          break;
        case if_icmpgeCK: replacement = makeCODEif_icmplt(stop_label2, NULL);
                          break;
        case if_icmpneCK: replacement = makeCODEif_icmpeq(stop_label2, NULL);
                          break;
        default: return 0;
      }
    } else {
      switch((*c)->kind)
      {
        case ifeqCK:      replacement = makeCODEifeq(stop_label2, NULL);
                          break;
        case ifneCK:      replacement = makeCODEifne(stop_label2, NULL);
                          break;
        case if_acmpeqCK: replacement = makeCODEif_acmpeq(stop_label2, NULL);
                          break;
        case if_acmpneCK: replacement = makeCODEif_acmpne(stop_label2, NULL);
                          break;
        case ifnullCK:    replacement = makeCODEifnull(stop_label2, NULL);
                          break;
        case ifnonnullCK: replacement = makeCODEifnonnull(stop_label2, NULL);
                          break;
        case if_icmpeqCK: replacement = makeCODEif_icmpeq(stop_label2, NULL);
                          break;
        case if_icmpgtCK: replacement = makeCODEif_icmpgt(stop_label2, NULL);
                          break;
        case if_icmpltCK: replacement = makeCODEif_icmplt(stop_label2, NULL);
                          break;
        case if_icmpleCK: replacement = makeCODEif_icmple(stop_label2, NULL);
                          break;
        case if_icmpgeCK: replacement = makeCODEif_icmpge(stop_label2, NULL);
                          break;
        case if_icmpneCK: replacement = makeCODEif_icmpne(stop_label2, NULL);
                          break;
        default: return 0;
      }
    }
    /*Increase the count of the label since it'll be dropped by replace_modified*/
    copylabel(stop_label2);  
    return replace_modified(c, 7, replacement);     
  }
  return 0;
}

/* dup
 * istore x
 * pop
 * -------->
 * istore x
 *
 * Explanation: Removes useless dup and pop.
 * At the end the stack should be empty ( from the point of view of this chunk of code).
 * And the value is stored at position x.
 */
int simplify_istore(CODE **c)
{ int x;
  if (is_dup(*c) &&
      is_istore(next(*c),&x) &&
      is_pop(next(next(*c)))) {
     return replace(c,3,makeCODEistore(x,NULL));
  }
  return 0;
}

/* istore x
 * iload x
 * -------->
 * dup
 * istore x
 *
 * Explanation: Tries to save 1 byte by using dup instead of iload. 
 * At the end, we have the same on both stacks.
 */
int simplify_istore_iload(CODE **c)
{ int x, y;
  if (is_istore(*c, &x) &&
      is_iload(next(*c), &y) &&
      x == y) {
     return replace(c,2,makeCODEdup(
                        makeCODEistore(x,NULL)));
  }
  return 0;
}

/*
* dup
* ifne true_#
* pop
* ...
* some_inst
* true_#: // Need to check that the label is unique
* ifeq some_label
* ------------------>
* ifne true_#
* ...
* some_inst
* ifeq some_label
* true_#:
*
* Explanation: In this case, the second if will fail without a doubt. So if the true label is unique, we will be
* swap it with the second if. By removing the dup and pop, the stack status will not be altered after this transformation.
*  
*/
int remove_unreacheable_if(CODE ** c)
{
  int true_label = 0, some_label = 0;
  CODE* ptr = *c;
  CODE* ifeq_inst = NULL;
  CODE* some_inst = *c;
  if(is_dup(ptr) &&
     is_ifne((ptr = next(ptr)), &true_label) && uniquelabel(true_label) &&
     is_pop((ptr = next(ptr))) &&
     is_ifeq( (ifeq_inst = next(destination(true_label))), &some_label)
     ) 
  {
    /*Swap the second if and the true label*/
    while(some_inst->next != destination(true_label)){
      some_inst = next(some_inst);
    }
    some_inst->next = ifeq_inst;
    destination(true_label)->next = next(ifeq_inst);
    ifeq_inst->next = destination(true_label);

    return replace(c, 3, makeCODEifne(true_label, NULL));
  }
  return 0;
}

/*
* dup
* ifeq true_#
* pop
* ...
* some_inst
* true_#:
* ifne some_label
* ------------------>
* ifeq true_#
* ...
* some_inst
* ifne some_label
* true_#:
*
* Explanation: In this case, the second if will fail without a doubt. So if the true label is unique, we will be
* swap it with the second if. By removing the dup and pop, the stack status will not be altered after this transformation.
*/
int remove_unreacheable_if_2(CODE ** c)
{
  int true_label = 0, some_label = 0;
  CODE* ptr = *c;
  CODE* ifne_inst = NULL;
  CODE* some_inst = *c;
  if(is_dup(ptr) &&
     is_ifeq((ptr = next(ptr)), &true_label) && uniquelabel(true_label) &&
     is_pop((ptr = next(ptr))) &&
     is_ifne( (ifne_inst = next(destination(true_label))), &some_label)
     ) 
  {
    /*Swap the second if and the true label*/
    while(some_inst->next != destination(true_label)){
      some_inst = next(some_inst);
    }
    some_inst->next = ifne_inst;
    destination(true_label)->next = next(ifne_inst);
    ifne_inst->next = destination(true_label);

    return replace(c, 3, makeCODEifeq(true_label, NULL));
  }
  return 0;
}

/*
* dup
* ifne true_#
* pop
* ...
* true_#:
* ifne some_label
* ------------------>
* ifne some_label // Increase ref count by 1
* ...
* true_#: //decrease ref count by 1
* ifne some_label
*
* Explanation: In this case, we know that the second if will evaluated to true for sure. 
* To simplify that, we will jump directly the second label if the first if evaluate to true. 
* By doing this, we can remove the dup and pop pair in order to keep the same expected stack elements. 
*/
int remove_extra_ifne(CODE ** c)
{
  int true_label = 0, some_label = 0;
  CODE* ptr = *c;
  if(is_dup(ptr) &&
     is_ifne((ptr = next(ptr)), &true_label) &&
     is_pop((ptr = next(ptr))) &&
     is_ifne( next(destination(true_label)), &some_label)
     ) 
  {
    droplabel(true_label);
    copylabel(some_label);
    return replace(c, 3, makeCODEifne(some_label, NULL));
  }

  return 0;
}

/*
* dup
* ifeq true_#
* pop
* ...
* true_#:
* ifeq some_label
* ------------------>
* ifeq some_label // Increase ref count by 1
* ...
* true_#: //decrease ref count by 1
* ifeq some_label
*
* Explanation: In this case, we know that the second if will evaluated to true for sure. 
* To simplify that, we will jump directly the second label if the first if evaluate to true. 
* By doing this, we can remove the dup and pop pair in order to keep the same expected stack elements. 
*/
int remove_extra_ifeq(CODE ** c)
{
  int true_label = 0, some_label = 0;
  CODE* ptr = *c;
  if(is_dup(ptr) &&
     is_ifeq((ptr = next(ptr)), &true_label) &&
     is_pop((ptr = next(ptr))) &&
     is_ifeq( next(destination(true_label)), &some_label)
     ) 
  {
    droplabel(true_label);
    copylabel(some_label);
    return replace(c, 3, makeCODEifeq(some_label, NULL));
  }

  return 0;
}

/*
*                                          [ , ,]
* new new_string                           [new_obj, ,]
* dup                                      [new_obj,new_obj ,]
* invokenonvirtual invokenonvirtual_string [new_obj , ,]
* dup                                      [new_obj ,new_obj ,]
* aload_#                                  [new_obj ,new_obj ,O1]
* swap                                     [new_obj ,O1 ,new_obj]
* putfield putfield_string;                [new_obj , ,]
* pop                                      [ , ,]
* -------------------------------------->
* aload_#                                  [O1, ,]
* new new_string                           [O1,new_obj ,]
* dup                                      [O1,new_obj ,new_obj]
* invokenonvirtual invokenonvirtual_string [O1,new_obj ,] 
* putfield putfield_string;                [ , ,]
*
* Explanation: As we can see from the stack trace, the stack remains the same and all the functions
* are called with the same arguments. 
* By changing the order of instructions, we can remove some extra dup, swap and pop.
*/
int restructure_field_assign_newobject(CODE** c)
{
  char* new_string = NULL;
  char* invokenonvirtual_string = NULL;
  char* putfield_string = NULL;
  int aload_number = 0;
  CODE* ptr = *c;
  CODE* replacement = NULL;
  if( is_new(ptr, &new_string) &&
      is_dup((ptr = next(ptr))) &&
      is_invokenonvirtual((ptr = next(ptr)), &invokenonvirtual_string) &&
      is_dup((ptr = next(ptr))) &&
      is_aload((ptr = next(ptr)), &aload_number) &&
      is_swap((ptr = next(ptr))) &&
      is_putfield((ptr = next(ptr)), &putfield_string) &&
      is_pop((ptr = next(ptr)))
    )
  {
    replacement = makeCODEaload(aload_number,  
                  makeCODEnew(new_string, 
                  makeCODEdup(
                  makeCODEinvokenonvirtual(invokenonvirtual_string, 
                  makeCODEputfield(putfield_string, NULL)))));
    
    return replace(c, 8, replacement);
  }
  return 0;
}

/*                          [,,,]
* ldc (str|int)             [(str|int),,,]
* dup                       [(str|int),(str|int),,]
* aload_#                   [(str|int),(str|int),old_obj,]
* swap                      [(str|int),old_obj,(str|int),,]
* putfield putfield_string  [(str|int),,,]
* pop                       [,,,]
* -------------------->
* aload_#                   [old_obj, , ]
* ldc (str|int)             [old_obj,(str|int) , ]
* putfield putfield_string  [,,,]
*
* Explanation: As we can see from the stack trace, the stack remains the same and all the functions
* are called with the same arguments. 
* By changing the order of instructions, we can remove some extra dup, swap and pop.
*/
int restructure_field_assign_strorint(CODE** c)
{
  char* putfield_string = NULL;
  char* ldc_string = NULL;
  int aload_number = 0;
  int ldc_int = 0;
  CODE* ptr = *c;
  CODE* replacement = NULL;
  if (
       (is_ldc_int(ptr, &ldc_int) || is_ldc_string(ptr, &ldc_string)) &&
       is_dup((ptr = next(ptr))) &&
       is_aload((ptr = next(ptr)), &aload_number) &&
       is_swap((ptr = next(ptr))) &&
       is_putfield((ptr = next(ptr)), &putfield_string) &&
       is_pop((ptr = next(ptr)))
     )
  {
    replacement = makeCODEputfield(putfield_string, NULL);
    if( (*c)->kind == ldc_stringCK){ /* ldc string*/
      replacement = makeCODEldc_string(ldc_string, replacement);
    } else { /* ldc int*/
      replacement = makeCODEldc_int(ldc_int, replacement);
    }
    replacement = makeCODEaload(aload_number, replacement);

    return replace(c, 6, replacement);
  }
  return 0;
}

/*                           [,,,]
* aload_#|iload_#            [element,,,]
* dup                        [element,element,,]
* aload_##                   [element,element,old_obj,]
* swap                       [element,old_obj,element,]
* putfield putfield_string   [element,,,]
* pop                        [,,,]
* -------------------->
* aload_##                   [old_obj, , ]
* aload_#|iload_#            [old_obj,element , ]
* putfield putfield_string   [,,,]
*
* Explanation: As we can see from the stack trace, the stack remains the same and all the functions
* are called with the same arguments. 
* By changing the order of instructions, we can remove some extra dup, swap and pop.
*/
int restructure_field_assign_oldvar(CODE** c)
{
  char* putfield_string = NULL;
  int load1_number = 0;
  int aload2_number = 0;
  CODE* ptr = *c;
  CODE* replacement = NULL;
  if (
       (is_aload(ptr, &load1_number) || is_iload(ptr, &load1_number) ) &&
       is_dup((ptr = next(ptr))) &&
       is_aload((ptr = next(ptr)), &aload2_number) &&
       is_swap((ptr = next(ptr))) &&
       is_putfield((ptr = next(ptr)), &putfield_string) &&
       is_pop((ptr = next(ptr)))
     )
  {
    replacement = makeCODEputfield(putfield_string, NULL);
    if((*c)->kind == aloadCK){
      replacement = makeCODEaload(load1_number, replacement);
    }else {
      replacement = makeCODEiload(load1_number, replacement);
    }
    replacement = makeCODEaload(aload2_number, replacement);

    return replace(c, 6, replacement);
  }
  return 0;
}

/* iload x
 * ldc k   (0<=k<=128) iinc have a byte for the immediate value
 * isub
 * istore x
 * --------->
 * iinc x -k
 *
 * Explanation: Similar to the positive increment pattern, this reduces the number of operations needed to decrement
 * a local variable. Since 'iinc' can only take a byte constant, k is limited between 0 and 128. 
 */ 
int negative_increment(CODE **c)
{ int x,y,k;
  if (is_iload(*c,&x) &&
      is_ldc_int(next(*c),&k) &&
      is_isub(next(next(*c))) &&
      is_istore(next(next(next(*c))),&y) &&
      x==y && 0<=k && k<=128) {
     return replace(c,4,makeCODEiinc(x,-k,NULL));
  }
  return 0;
}

/*                         [obj, ]
* aconst_null              [obj,null, ]
* if_acmp(eq|ne) label     [,, ]
* ------------------->
*                          [obj, ]
* if_(null|nonnull) label  [,, ]
*
* Explanation: This pattern simplifies null comparison by removing the need to use if_acmp and loading null.
*/
int simplify_null_comparison(CODE** c)
{
  int label = -1;
  if(is_aconst_null(*c)&&
     is_if_acmpeq(next(*c), &label)
     )
  {
    return replace(c, 2, makeCODEifnull(label, NULL));
  }

  if(is_aconst_null(*c)&&
     is_if_acmpne(next(*c), &label)
     )
  {
    return replace(c, 2, makeCODEifnonnull(label, NULL));
  }

  return 0;
}

/*                      [int, ]
* iconst_0              [int,0 ]
* if_icmp(eq|ne) label  [,]
* ------------------->
*                       [int,]
* if_(eq|ne) label      [,]
*
* Explanation: This pattern simplifies comparison with zero by removing the need to use if_icmp and loading a 0.
*/
int simplify_zero_comparison(CODE** c)
{
  int label = -1, num = -1;
  if(is_ldc_int(*c, &num) && num ==0 && 
     is_if_icmpeq(next(*c), &label)
     )
  {
    return replace(c, 2, makeCODEifeq(label, NULL));
  }
  
  if(is_ldc_int(*c, &num) && num ==0 && 
     is_if_icmpne(next(*c), &label)
     )
  {
    return replace(c, 2, makeCODEifne(label, NULL));
  }

  return 0;
}

/*
*  ifnull label1 
*  goto label2 
*  label1: 
*  ------------------->
*  ifnonnull label2 
*  label1:          // Decrease the reference count by 1
*  
* Explanation: In this code, we can reduce the number of instruction by changing the if condition. 
* The modifications give the same result logic wise.
*/
int simplify_null_comparison_2(CODE** c)
{
  int label1 = -1, label2 = -1, temp = -1;
  if( 
      is_ifnull(*c, &label1) &&
      is_goto(next(*c), &label2) &&
      is_label(next(next(*c)), &temp) && temp == label1
    )
  {
    droplabel(label1);
    return replace(c, 2, makeCODEifnonnull(label2, NULL));
  }

  return 0;
}

/*
*  label: // dead label 
*  ------------>
*  -
*
* Explanation: We decided to remove the dead labels because they might be blocking other patterns from executing.
*/
int remove_dead_label(CODE** c)
{
  int label = -1;
  if( is_label(*c, &label) && deadlabel(label) )
  {
    return kill_line(c);
  }
  return 0;
}

/*
*  nop: // useless
*  ------------>
*  -
*
* Explanation: We did not see any need for nop in the code.
*/
int remove_nop(CODE** c)
{
  if( is_nop(*c) )
  {
    return kill_line(c);
  }
  return 0;
}

/*
*  return
*  not a label // unreacheable 
*  ---------> 
*  return 
*
* Explanation: Since all the code after a return will not be reached unless it's a label, we remove all of them.
*/
int remove_code_after_return(CODE** c)
{
  int label = -1;
  CODE* ptr = next(*c);
  int count = 1;
  CODE* replacement = NULL;
  if((is_ireturn(*c) || is_areturn(*c) || is_return(*c)))
  {
    while(ptr != NULL && !is_label(ptr, &label)){
      count++;
      ptr = next(ptr);
    }

    if(count == 1)
      return 0;

    switch ((*c)->kind)
    {
      case ireturnCK: replacement = makeCODEireturn(NULL);
                      break;
      case areturnCK: replacement = makeCODEareturn(NULL);
                      break;
      case returnCK:  replacement = makeCODEreturn(NULL);
                      break;
      default: return 0;
    }
    
    return replace_modified(c, count, replacement);
  }
  return 0;
}

/*
* goto label
* ...
* label:
* [a|i]return
* -------------->
* [a|i]return
* ...
* label: // hopefully this label will be dead and will be dropped by another pattern
* [a|i]return
*
* Explanation: Instead of jumping to the return using the goto, we can just return directly.
* This has the same effect of the stack and the program logic.
*/
int remove_extra_goto_before_return(CODE** c)
{
  int label = -1;
  CODE* ptr = NULL;
  CODE* replacement = NULL;
  if(is_goto(*c, &label) && 
     (is_ireturn(ptr = next(destination(label))) || is_areturn(ptr) || is_return(ptr)))
  {
    switch (ptr->kind)
    {
      case ireturnCK: replacement = makeCODEireturn(NULL);
                      break;
      case areturnCK: replacement = makeCODEareturn(NULL);
                      break;
      case returnCK:  replacement = makeCODEreturn(NULL);
                      break;
      default: return 0;
    }  

    return replace_modified(c, 1, replacement); 
  }
  return 0;
}

/*                    [,,]
* ldc const_str       [str_obj,,]
* dup                 [str_obj,str_obj,]
* ifnonnull label     [str_obj,,] -> branch takes
* pop                 [,,] // not reached
* ldc "null"          [,,] // not reached
* ------------------->
* ldc const_str       [str_obj,,] 
* goto label          [str_obj,,] ->branch_taken
*
*
* Note: The label references wont change so we wont be using: drop, copy or replace_modified
*
* Explanation: When loading a constant string, we know that the object will not be null so there is no need to add
* the extra checks.
*/
int simplify_const_str_load(CODE** c)
{
  CODE* ptr = *c;
  CODE* replacement = NULL;
  int label = -1;
  char* const_str = NULL;
  char* temp_str = NULL;
  if( is_ldc_string(ptr, &const_str) &&
      is_dup((ptr = next(ptr))) &&
      is_ifnonnull((ptr = next(ptr)), &label) &&
      is_pop((ptr = next(ptr))) &&
      is_ldc_string((ptr = next(ptr)), &temp_str) && strcmp(temp_str, "null") == 0
    )
  {
    replacement = makeCODEldc_string(const_str, makeCODEgoto(label, NULL));
    return replace(c, 5, replacement);
  }
  return 0;
}

/*
* goto label
* label:
* -------->
* label:
*  
* Note: This is due to pattern "simplify_const_str_load". I did not want to merge the two in
* case another pattern give this issue.
*/
int remove_useless_goto(CODE** c)
{
  int label1 = -1;
  int label2 = -1;
  if( is_goto(*c, &label1) &&
      is_label(next(*c), &label2) && label1 == label2 
    )
  { 
      return replace_modified(c, 1, NULL); /* only remove the goto */  
  }
  return 0;
}

/*                         [,,]
* aconst_null              [null,,]
* dup                      [null,null,] 
* aload_##                 [null,null,obj]
* swap                     [null,obj,null]
* putfield putfield_string [null,,]
* pop                      [,,]
* -------------------->
*                          [,,]
* aload_##                 [obj,,]
* aconst_null              [obj,null,]
* putfield putfield_string [,,]
*
* Explanation: As we can see from the stack trace, the stack remains the same and all the functions
* are called with the same arguments. 
* By changing the order of instructions, we can remove some extra dup, swap and pop.
*/
int restructure_field_assign_nullobj(CODE** c)
{
  char* putfield_string = NULL;
  int aload_number = 0;
  CODE* ptr = *c;
  CODE* replacement = NULL;
  if (
       is_aconst_null(ptr) &&
       is_dup((ptr = next(ptr))) &&
       is_aload((ptr = next(ptr)), &aload_number) &&
       is_swap((ptr = next(ptr))) &&
       is_putfield((ptr = next(ptr)), &putfield_string) &&
       is_pop((ptr = next(ptr)))
     )
  {
    replacement = makeCODEputfield(putfield_string, NULL);
    replacement = makeCODEaconst_null( replacement);
    replacement = makeCODEaload(aload_number, replacement);
    return replace(c, 6, replacement);
  }
  return 0;
}

void init_patterns(void) {
	ADD_PATTERN(simplify_multiplication_right);
	ADD_PATTERN(simplify_astore);
	ADD_PATTERN(simplify_astore_aload);
	ADD_PATTERN(positive_increment);
	ADD_PATTERN(simplify_goto_goto);
  ADD_PATTERN(simplify_if_stmt);
  ADD_PATTERN(simplify_istore);
	ADD_PATTERN(simplify_istore_iload);
  ADD_PATTERN(remove_unreacheable_if);
  ADD_PATTERN(remove_unreacheable_if_2);
  ADD_PATTERN(remove_extra_ifne);
  ADD_PATTERN(restructure_field_assign_newobject);
  ADD_PATTERN(restructure_field_assign_strorint);
  ADD_PATTERN(restructure_field_assign_oldvar);
  ADD_PATTERN(negative_increment);
  ADD_PATTERN(simplify_null_comparison);
  ADD_PATTERN(simplify_zero_comparison);
  ADD_PATTERN(remove_extra_ifeq);
  ADD_PATTERN(simplify_null_comparison_2);
  ADD_PATTERN(remove_dead_label);
  ADD_PATTERN(remove_nop);
  ADD_PATTERN(remove_code_after_return);
  ADD_PATTERN(remove_extra_goto_before_return);
  ADD_PATTERN(simplify_const_str_load);
  ADD_PATTERN(remove_useless_goto);
  ADD_PATTERN(restructure_field_assign_nullobj);
}
