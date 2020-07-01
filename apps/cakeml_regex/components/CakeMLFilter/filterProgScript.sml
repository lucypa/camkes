(*
 * Copyright 2018, Data61
 * Commonwealth Scientific and Industrial Research Organisation (CSIRO)
 * ABN 41 687 119 230.
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(DATA61_BSD)
 *)

open preamble MapProgTheory ml_translatorLib ml_progLib basisFunctionsLib ml_translatorTheory
     charsetTheory regexpTheory regexp_parserTheory regexp_compilerTheory cfTacticsBaseLib
     camkesStartTheory;

val _ = temp_delsimps ["NORMEQ_CONV"]

val _ = new_theory "filterProg";

(*---------------------------------------------------------------------------*)
(* The regexp wrt. which we're filtering                                     *)
(* Read from the cmakeConstants SML module which is generated by the         *)
(* build system.                                                             *)
(*---------------------------------------------------------------------------*)

val the_regexp = camkesConstants.filter_regex;

val _ = translation_extends "camkesStart";

(*---------------------------------------------------------------------------*)
(* Reuse of some code from regexpLib, so that some intermediate lemmas are   *)
(* kept for use in the proofs of side-condition theorems arising from        *)
(* translation.                                                              *)
(*---------------------------------------------------------------------------*)

val regexp_compilation_results as {certificate, aux, ...}
  = regexpLib.gen_dfa regexpLib.HOL (Regexp_Type.fromString the_regexp);

val matcher_certificate = save_thm
  ("matcher_certificate",
    certificate
      |> valOf
      |> CONV_RULE(QUANT_CONV(LHS_CONV (REWRITE_CONV [MAP])))
);

(*---------------------------------------------------------------------------*)
(* Define a named matcher function                                           *)
(*---------------------------------------------------------------------------*)

val matcher_def =
 Define `matcher ^(matcher_certificate |> concl |> dest_forall |> fst) =
                 ^(matcher_certificate |> concl |> dest_forall |> snd |> lhs)`

val match_string_def = Define `match_string s = matcher(explode s)`

val language_def =
 Define `language =
                 ^(matcher_certificate |> concl |> dest_forall |> snd |> rhs |> rator)`

val match_string_eq = Q.prove(`match_string = language o explode`,
  `!s. match_string s = (language o explode) s` suffices_by metis_tac[]
  >> rw[match_string_def,language_def,matcher_def,matcher_certificate]);

val IMPLODE_pointless = Q.prove(`!s. IMPLODE s = s`,
  Induct >> simp[]);

val THE_lift = Q.prove(`!f r.
  IS_SOME r ==> THE(lift f r) = f(THE r)`,
  Cases_on `r` >> simp[]);

(*---------------------------------------------------------------------------*)
(* Translator setup boilerplate                                              *)
(*---------------------------------------------------------------------------*)

fun def_of_const tm = let
  val res = dest_thy_const tm handle HOL_ERR _ =>
              failwith ("Unable to translate: " ^ term_to_string tm)
  val name = (#Name res)
  fun def_from_thy thy name =
    DB.fetch thy (name ^ "_def") handle HOL_ERR _ =>
    DB.fetch thy (name ^ "_DEF") handle HOL_ERR _ =>
    DB.fetch thy (name ^ "_thm") handle HOL_ERR _ =>
    DB.fetch thy name
  val def = def_from_thy (#Thy res) name handle HOL_ERR _ =>
            failwith ("Unable to find definition of " ^ name)
  in def end

val _ = find_def_for_const := def_of_const;


(* TODO: translate balanced_map module separately? *)

val _ = ml_translatorLib.pick_name :=
  let val default = !ml_translatorLib.pick_name in
    fn c =>
    if same_const c ``balanced_map$member`` then "balanced_map_member" else
    if same_const c ``balanced_map$empty`` then "balanced_map_empty" else
      default c
  end

val spec64 = INST_TYPE[alpha|->``:64``]

val _ = translate matcher_def

val mem_tolist = Q.prove(`MEM (toList l) (MAP toList ll) = MEM l ll`,
  Induct_on `ll` >> fs[]);

val length_tolist_cancel = Q.prove(
  `!n. n < LENGTH l ==> LENGTH (EL n (MAP mlvector$toList l)) = length (EL n l)`,
  Induct_on `l`
  >> fs[]
  >> rpt strip_tac
  >> Cases_on `n`
  >> fs[mlvectorTheory.length_toList]);

val EL_map_toList = Q.prove(`!n. n < LENGTH l ==> EL n' (EL n (MAP toList l)) = sub (EL n l) n'`,
  Induct_on `l`
  >> fs[]
  >> rpt strip_tac
  >> Cases_on `n`
  >> fs[mlvectorTheory.EL_toList]);

val exec_dfa_side_imp = Q.prove(
  `!finals table n s.
   good_vec (MAP toList (toList table)) (toList finals)
    /\ EVERY (λc. MEM (ORD c) ALPHABET) (EXPLODE s)
    /\ n < length finals
   ==> exec_dfa_side finals table n s`,
  Induct_on `s`
  >- fs[fetch "-" "exec_dfa_side_def"]
  >> PURE_ONCE_REWRITE_TAC [fetch "-" "exec_dfa_side_def"]
  >> fs[good_vec_def,mlvectorTheory.length_toList]
  >> rpt GEN_TAC
  >> Induct_on `table`
   >> rpt strip_tac
   >> fs[sub_def,length_def,mlvectorTheory.toList_thm]
   >> `MEM (toList (EL n l)) (MAP toList l)`
        by fs[EL_MEM,mem_tolist,mlvectorTheory.toList_thm]
   >- metis_tac[mlvectorTheory.length_toList]
   >> first_x_assum(MATCH_MP_TAC o Q.SPECL [`finals`,`Vector l`, `x1`])
    >> rpt strip_tac
    >> fs[mlvectorTheory.toList_thm, length_def, mem_tolist]
    >- metis_tac[]
    >> first_x_assum(ASSUME_TAC o Q.SPECL [`toList (EL n l)`,`ORD h`])
    >> first_x_assum(MATCH_MP_TAC o Q.SPECL [`n`,`ORD h`,`x1`])
    >> rfs[mlvectorTheory.length_toList,mem_tolist,EL_map_toList,length_tolist_cancel]);

val all_ord_string = Q.prove
(`EVERY (\c. MEM (ORD c) ALPHABET) s
   <=>
  EVERY (\c. ORD c < alphabet_size) s`,
 simp_tac list_ss [mem_alphabet_iff]);

val good_vec_thm =
 SIMP_RULE std_ss [dom_Brz_alt_equal]
    regexp_compilerTheory.compile_regexp_good_vec;

val matcher_side_lem = Q.prove(
  `!s. matcher_side s <=> T`,
  rw[fetch "-" "matcher_side_def"]
  >> match_mp_tac exec_dfa_side_imp
  >> rw_tac list_ss [mlvectorTheory.toList_thm]
    >- metis_tac [aux |> valOf,good_vec_thm]
    >- rw_tac list_ss [all_ord_string,ORD_BOUND,alphabet_size_def]
    >- EVAL_TAC)
 |>
 update_precondition;

val _ = translate match_string_def

(* val _ = translate(word_bit_test |> spec64); *)

(* TODO: this is largely copied from the bootstrap translation
         can it be properly abstracted out? *)
local
  val ths = ml_translatorLib.eq_lemmas();
in
  fun find_equality_type_thm tm =
    first (can (C match_term tm) o rand o snd o strip_imp o concl) ths
end

val EqualityType_WORD = find_equality_type_thm``WORD``
val no_closures_def = ml_translatorTheory.no_closures_def;
val LIST_TYPE_def = ml_translatorTheory.LIST_TYPE_def;
val EqualityType_def = ml_translatorTheory.EqualityType_def;
val types_match_def = ml_translatorTheory.types_match_def;
val ctor_same_type_def = semanticPrimitivesTheory.ctor_same_type_def;

Theorem tolist_fromlist_map_cancel:
  MAP mlvector$toList (MAP fromList ll) = ll
Proof
  Induct_on `ll` >> fs[]
QED

(*---------------------------------------------------------------------------*)
(* Auxiliary functions to deal with null termination.                        *)
(*---------------------------------------------------------------------------*)

val null_index_def = tDefine "null_index"
  `null_index s n =
    if n >= strlen s then NONE
    else if strsub s n = CHR 0 then SOME n
    else null_index s (SUC n)`
  (wf_rel_tac `inv_image (measure (λ(a,b). SUC a - b)) (strlen ## I)`);

val null_index_ind = fetch "-" "null_index_ind";

Theorem null_index_le_len:
  !s n m. null_index s n = SOME m ==> m < strlen s
Proof
  ho_match_mp_tac null_index_ind
  >> rpt strip_tac
  >> qhdtm_x_assum `null_index` (mp_tac o PURE_ONCE_REWRITE_RULE [null_index_def])
  >> rw[]
QED

Theorem null_index_in_bound:
  !s n m. null_index s n = SOME m ==> n <= m
Proof
  ho_match_mp_tac null_index_ind
  >> rpt strip_tac
  >> qhdtm_x_assum `null_index` (mp_tac o PURE_ONCE_REWRITE_RULE [null_index_def])
  >> rw[] >> fs[]
QED

Theorem null_index_null:
  !s n m. null_index s n = SOME m ==> strsub s m = CHR 0
Proof
  ho_match_mp_tac null_index_ind
  >> rpt strip_tac
  >> qhdtm_x_assum `null_index` (mp_tac o PURE_ONCE_REWRITE_RULE [null_index_def])
  >> rw[] >> fs[]
QED

Theorem null_index_no_null:
  !s n m. null_index s n = SOME m ==> EVERY ($~ o $= (CHR 0)) (TAKE (m-n) (DROP n (explode s)))
Proof
  ho_match_mp_tac null_index_ind
  >> rpt strip_tac
  >> qhdtm_x_assum `null_index` (mp_tac o PURE_ONCE_REWRITE_RULE [null_index_def])
  >> rw[]
  >> first_x_assum drule >> rpt(disch_then drule)
  >> strip_tac
  >> imp_res_tac null_index_le_len
  >> imp_res_tac null_index_in_bound
  >> qmatch_goalsub_abbrev_tac `EVERY _ (TAKE a1 a2)`
  >> Q.ISPECL_THEN [`a1`,`1`,`a2`] mp_tac take_drop_partition
  >> unabbrev_all_tac
  >> impl_tac >- intLib.COOPER_TAC
  >> disch_then(fn x => rw[GSYM x])
  >> fs[ADD1,DROP_DROP]
  >> `n < LENGTH(explode s)`
     by(fs[])
  >> drule TAKE1_DROP
  >> Cases_on `s` >> fs[mlstringTheory.strsub_def]
QED

Theorem null_index_no_null2:
  !s n. null_index s n = NONE ==> EVERY ($~ o $= (CHR 0)) (DROP n (explode s))
Proof
  ho_match_mp_tac null_index_ind
  >> rpt strip_tac
  >> qhdtm_x_assum `null_index` (mp_tac o PURE_ONCE_REWRITE_RULE [null_index_def])
  >> rw[] >> Cases_on `n ≥ strlen s`
  >> Cases_on `s` >> fs[GREATER_EQ]
  >> imp_res_tac DROP_LENGTH_TOO_LONG >> fs[]
  >> `n < STRLEN s'` by fs[]
  >> imp_res_tac DROP_CONS_EL >> fs[]
QED

val cut_at_null_def = Define `cut_at_null s =
  case null_index s 0 of
      NONE => strcat s (str(CHR 0))
    | SOME n => substring s 0 (SUC n)`

Theorem cut_at_null_SPLITP:
  !s. cut_at_null s = implode(FST(SPLITP ($= (CHR 0)) (explode s)) ++ [CHR 0])
Proof
  Cases >> rw[cut_at_null_def] >> reverse(PURE_TOP_CASE_TAC >> rw[])
  >- (imp_res_tac null_index_le_len >> rw[mlstringTheory.substring_def]
      >> fs[mlstringTheory.strlen_def,mlstringTheory.implode_def]
      >> imp_res_tac null_index_no_null >> fs[]
      >> imp_res_tac null_index_null >> fs[]
      >> imp_res_tac SPLITP_TAKE_DROP >> rfs[]
      >> imp_res_tac (GSYM TAKE_SEG) >> fs[]
      >> fs[ADD1]
      >> Q.ISPECL_THEN [`s'`,`x`] assume_tac TAKE_EL_SNOC
      >> rfs[])
  >- (imp_res_tac null_index_no_null2
      >> fs[o_DEF] >> imp_res_tac SPLITP_EVERY
      >> fs[mlstringTheory.implode_def,mlstringTheory.strcat_thm])
QED

val _ = translate cut_at_null_def;

val null_index_side_lem = Q.prove(
  `!s n. null_index_side s n <=> T`,
  ho_match_mp_tac null_index_ind
  >> rw[]
  >> PURE_ONCE_REWRITE_TAC[fetch "-" "null_index_side_def"]
  >> fs[ADD1])
 |> update_precondition;

val cut_at_null_side_lem = Q.prove(`!s. cut_at_null_side s <=> T`,
  rw[fetch "-" "cut_at_null_side_def",null_index_side_lem]
  >> imp_res_tac null_index_le_len >> fs[])
 |> update_precondition;

val null_index_w_def = tDefine "null_index_w"
  `null_index_w s n =
    if n >= LENGTH s then NONE
    else if EL n s = 0w then SOME n
    else null_index_w s (SUC n)`
  (wf_rel_tac `inv_image (measure (λ(a,b). SUC a - b)) (LENGTH ## I)`);

val null_index_w_ind = fetch "-" "null_index_w_ind";

Theorem null_index_w_le_len:
  !s n m. null_index_w s n = SOME m ==> m < LENGTH s
Proof
  ho_match_mp_tac null_index_w_ind
  >> rpt strip_tac
  >> qhdtm_x_assum `null_index_w` (mp_tac o PURE_ONCE_REWRITE_RULE [null_index_w_def])
  >> rw[]
QED

Theorem null_index_w_in_bound:
  !s n m. null_index_w s n = SOME m ==> n <= m
Proof
  ho_match_mp_tac null_index_w_ind
  >> rpt strip_tac
  >> qhdtm_x_assum `null_index_w` (mp_tac o PURE_ONCE_REWRITE_RULE [null_index_w_def])
  >> rw[] >> fs[]
QED

Theorem null_index_w_null:
  !s n m. null_index_w s n = SOME m ==> EL m s = 0w
Proof
  ho_match_mp_tac null_index_w_ind
  >> rpt strip_tac
  >> qhdtm_x_assum `null_index_w` (mp_tac o PURE_ONCE_REWRITE_RULE [null_index_w_def])
  >> rw[] >> fs[]
QED

Theorem null_index_w_no_null:
  !s n m. null_index_w s n = SOME m ==> EVERY ($~ o $= 0w) (TAKE (m-n) (DROP n s))
Proof
  ho_match_mp_tac null_index_w_ind
  >> rpt strip_tac
  >> qhdtm_x_assum `null_index_w` (mp_tac o PURE_ONCE_REWRITE_RULE [null_index_w_def])
  >> rw[]
  >> first_x_assum drule >> rpt(disch_then drule)
  >> strip_tac
  >> imp_res_tac null_index_w_le_len
  >> imp_res_tac null_index_w_in_bound
  >> qmatch_goalsub_abbrev_tac `EVERY _ (TAKE a1 a2)`
  >> Q.ISPECL_THEN [`a1`,`1`,`a2`] mp_tac take_drop_partition
  >> unabbrev_all_tac
  >> impl_tac >- intLib.COOPER_TAC
  >> disch_then(fn x => rw[GSYM x])
  >> fs[ADD1,DROP_DROP]
  >> `n < LENGTH s`
     by(fs[])
  >> drule TAKE1_DROP >> fs[]
QED

Theorem null_index_w_no_null2:
  !s n. null_index_w s n = NONE ==> EVERY ($~ o $= 0w) (DROP n s)
Proof
  ho_match_mp_tac null_index_w_ind
  >> rpt strip_tac
  >> qhdtm_x_assum `null_index_w` (mp_tac o PURE_ONCE_REWRITE_RULE [null_index_w_def])
  >> rw[] >> Cases_on `n ≥ LENGTH s`
  >> fs[GREATER_EQ]
  >> imp_res_tac DROP_LENGTH_TOO_LONG >> fs[]
  >> `n < LENGTH s` by fs[]
  >> imp_res_tac DROP_CONS_EL >> fs[]
QED

val cut_at_null_w_def = Define `cut_at_null_w s =
  case null_index_w s 0 of
      NONE => s ++ [0w]
    | SOME n => SEG (SUC n) 0 s`

Theorem cut_at_null_w_SPLITP:
  !s. cut_at_null_w s = FST(SPLITP ($= 0w) s) ++ [0w]
Proof
  rw[cut_at_null_w_def] >> reverse(PURE_TOP_CASE_TAC >> rw[])
  >- (imp_res_tac null_index_w_le_len >> rw[mlstringTheory.substring_def]
      >> fs[mlstringTheory.strlen_def,mlstringTheory.implode_def]
      >> imp_res_tac null_index_w_no_null >> fs[]
      >> imp_res_tac null_index_w_null >> fs[]
      >> imp_res_tac SPLITP_TAKE_DROP >> rfs[]
      >> fs[GSYM TAKE_SEG,ADD1]
      >> Q.ISPECL_THEN [`s`,`x`] assume_tac TAKE_EL_SNOC
      >> rfs[])
  >- (imp_res_tac null_index_w_no_null2
      >> fs[o_DEF] >> imp_res_tac SPLITP_EVERY >> fs[])
QED

Theorem null_index_w_thm:
  ∀s n. null_index_w (s:word8 list) n = null_index (implode (MAP (CHR ∘ w2n) s)) n
Proof
  ho_match_mp_tac null_index_w_ind
  >> rpt strip_tac
  >> MAP_EVERY PURE_ONCE_REWRITE_TAC [[null_index_def],[null_index_w_def]] >> rw[]
  >> fs[mlstringTheory.implode_def]
  >> `n < LENGTH s` by fs[]
  >> rfs[EL_MAP]
  >> qspecl_then [`[EL n s]`,`[0w]`] assume_tac MAP_CHR_w2n_11
  >> fs[]
QED

Theorem cut_at_null_w_thm:
  ∀s. cut_at_null_w (s:word8 list) = MAP (n2w o ORD) (explode (cut_at_null (implode (MAP (CHR ∘ w2n) s))))
Proof
  rw[cut_at_null_w_def,cut_at_null_def,null_index_w_thm]
  >> PURE_TOP_CASE_TAC >> rw[MAP_MAP_o]
  >> fs[n2w_ORD_CHR_w2n]
  >> imp_res_tac null_index_le_len
  >> fs[mlstringTheory.implode_def,mlstringTheory.substring_def]
  >> MAP_EVERY PURE_ONCE_REWRITE_TAC [[null_index_def],[null_index_w_def]] >> rw[]
  >> fs[GSYM TAKE_SEG,MAP_TAKE,MAP_MAP_o,n2w_ORD_CHR_w2n]
QED

Theorem cut_at_null_thm:
  cut_at_null(strlit (MAP (CHR o w2n) l)) = strlit(MAP (CHR o w2n) (cut_at_null_w(l:word8 list)))
Proof
  rw[cut_at_null_w_thm,MAP_MAP_o,implode_def,CHR_w2n_n2w_ORD,REWRITE_RULE[implode_def] implode_explode]
QED

val null_terminated_def = Define `
  null_terminated s = ?m. null_index s 0 = SOME m`

val null_terminated_w_def = Define `
  null_terminated_w s = ?m. null_index_w s 0 = SOME m`

Theorem null_terminated_w_thm:
  !s. null_terminated_w (s:word8 list) = null_terminated(implode(MAP (CHR o w2n) s))
Proof
  rw[null_terminated_def,null_terminated_w_def,null_index_w_thm]
QED

Theorem null_index_strcat1:
  !s1 n s2 m. null_index s1 n = SOME m ==> null_index (strcat s1 s2) n = SOME m
Proof
  ho_match_mp_tac null_index_ind
  >> rpt strip_tac
  >> qhdtm_x_assum `null_index` mp_tac
  >> PURE_ONCE_REWRITE_TAC [null_index_def]
  >> rw[] >> fs[]
  >> MAP_EVERY Cases_on [`s1`,`s2`]
  >> fs[mlstringTheory.strsub_def,mlstringTheory.strcat_def,mlstringTheory.concat_def,EL_APPEND_EQN]
QED

Theorem null_terminated_cut_APPEND:
  !s1 s2. null_terminated s1 ==> cut_at_null(strcat s1 s2) = cut_at_null s1
Proof
  rw[null_terminated_def,cut_at_null_def] >> imp_res_tac null_index_strcat1
  >> fs[] >> imp_res_tac null_index_le_len
  >> MAP_EVERY Cases_on [`s1`,`s2`]
  >> fs[mlstringTheory.strsub_def,mlstringTheory.strcat_def,mlstringTheory.concat_def,
        mlstringTheory.substring_def]
  >> match_mp_tac SEG_APPEND1 >> fs[]
QED

Theorem null_terminated_cut_w_APPEND:
  !s1 s2. null_terminated_w(s1:word8 list) ==> cut_at_null_w(s1 ++ s2) = cut_at_null_w s1
Proof
  rw[null_terminated_w_thm,cut_at_null_w_thm]
  >> drule null_terminated_cut_APPEND
  >> disch_then(qspec_then `implode(MAP (CHR ∘ w2n) s2)` assume_tac)
  >> simp[mlstringTheory.implode_STRCAT]
QED

val _ = export_theory ();
