// Copyright Epic Games, Inc. All Rights Reserved.

import 'package:epic_common/localizations.dart';
import 'package:epic_common/preferences.dart';
import 'package:epic_common/theme.dart';
import 'package:epic_common/widgets.dart';
import 'package:flutter/material.dart';
import 'package:flutter_gen/gen_l10n/app_localizations.dart';
import 'package:provider/provider.dart';

import '../connect/connect.dart';

/// Data types for EULA content.
enum _EulaDataType { paragraph, header, sub }

/// Screen for rendering and displaying content of the EULA doc.
class EulaScreen extends StatefulWidget {
  const EulaScreen({Key? key}) : super(key: key);

  static const String route = '/eula';

  @override
  State<EulaScreen> createState() => _EulaScreenState();
}

class _EulaScreenState extends State<EulaScreen> {
  /// Persistent store state reference.
  late PreferencesBundle store;

  /// whether the user agrees and accepts the EULA agreement or not.
  late bool bHasAcceptedEula;

  @override
  void initState() {
    super.initState();
    store = Provider.of<PreferencesBundle>(context, listen: false);
    bHasAcceptedEula = store.persistent.getBool('common.bHasAcceptedEula', defaultValue: false).getValue();
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: Text(
          AppLocalizations.of(context)!.eulaAppBarTitle,
          style: Theme.of(context).textTheme.displayLarge,
        ),
        backgroundColor: UnrealColors.gray10,
      ),
      body: EpicScrollView(
        child: Column(
          children: [
            Padding(
              padding: const EdgeInsets.symmetric(vertical: 20.0),
              child: Text(
                'Unreal Stage End User License Agreement',
                style: Theme.of(context).textTheme.displayLarge,
              ),
            ),
            ListView.builder(
              physics: NeverScrollableScrollPhysics(),
              itemCount: EulaDTO.data.length,
              shrinkWrap: true,
              itemBuilder: (BuildContext context, int index) {
                return _EulaItem(data: EulaDTO.data[index]);
              },
            ),
          ],
        ),
      ),
      bottomNavigationBar: Container(
        padding: EdgeInsets.symmetric(vertical: 18, horizontal: 18),
        decoration: BoxDecoration(color: UnrealColors.gray10),
        child: EpicGenericButton(
          color: UnrealColors.highlightBlue,
          child: Text(
            bHasAcceptedEula
                ? EpicCommonLocalizations.of(context)!.menuButtonOK
                : AppLocalizations.of(context)!.eulaAcceptButtonLabel,
          ),
          onPressed: () {
            if (bHasAcceptedEula) {
              if (Navigator.of(context).canPop()) {
                Navigator.pop(context);
              }
            } else {
              store.persistent.setBool('common.bHasAcceptedEula', true);
              Navigator.pushReplacementNamed(context, ConnectScreen.route);
            }
          },
        ),
      ),
    );
  }
}

/// A widget for visually representing heading and paragraphs on the EULA docs.
class _EulaItem extends StatelessWidget {
  const _EulaItem({required this.data, Key? key}) : super(key: key);

  /// Data objects for diff paragraphs in the EULA doc.
  final EulaDTO data;

  @override
  Widget build(BuildContext context) {
    return Container(
      padding: EdgeInsets.symmetric(horizontal: 16, vertical: 8),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          data.type == _EulaDataType.header
              ? Text(data.text, style: Theme.of(context).textTheme.displayLarge)
              : ParsedRichText(data.text),
          ...(data.subs ?? []).map<Widget>((e) => buildSubText(e, context)).toList(),
        ],
      ),
    );
  }

  /// Builds sub texts under some paragraphs using info from [data] under the current [context].
  Widget buildSubText(EulaDTO data, BuildContext context) {
    return Container(
      padding: EdgeInsets.only(left: 20).add(EdgeInsets.symmetric(vertical: 8)),
      child: Row(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Text(data.numbering!, style: Theme.of(context).textTheme.displayMedium),
          SizedBox(width: 10),
          Expanded(child: Text(data.text, style: Theme.of(context).textTheme.displayMedium)),
        ],
      ),
    );
  }
}

/// EULA data transfer object.
class EulaDTO {
  /// if the text being processed is a header, sub text or paragraph.
  _EulaDataType? type = _EulaDataType.header;

  /// Text to be rendered.
  String text;

  /// Sub text under a paragraph [text] when [type] = [_EulaDataType.sub].
  List<EulaDTO>? subs;

  /// optionally needed for numbered sub text [subs].
  String? numbering;

  EulaDTO({this.type, required this.text, this.subs, this.numbering});

  /// Data source for EULA docs.
  static final data = <EulaDTO>[
    EulaDTO(
      text:
          'Please read this Agreement carefully.  It is a legal document that explains your rights and obligations related to your use of the Software, including any Services you access through the Software.  By downloading or using the Software, or by otherwise indicating your acceptance of this Agreement, you are agreeing to be bound by the terms of this Agreement.  If you do not or cannot agree to the terms of this Agreement, please do not download or use this Software.',
      type: _EulaDataType.paragraph,
    ),
    EulaDTO(
      text:
          'Your use of the Services is also governed by Epic’s Terms of Service, which may be found at https://www.epicgames.com/tos and Epic’s Privacy Policy, which may be found at https://www.epicgames.com/privacypolicy. By downloading or using the Software, you also agree to Epic’s Terms of Service and Privacy Policy.',
      type: _EulaDataType.paragraph,
    ),
    EulaDTO(
      text:
          'Certain words or phrases are defined to have certain meanings when used in this Agreement.  Those words and phrases are defined below in Section 14.',
      type: _EulaDataType.paragraph,
    ),
    EulaDTO(
      text:
          'If your primary residence is in the United States of America, your agreement is with Epic Games, Inc.  If it is not in the United States of America, your agreement is with Epic Games Commerce GmbH.',
      type: _EulaDataType.paragraph,
    ),
    EulaDTO(
      text: '1. License Grant',
      type: _EulaDataType.header,
    ),
    EulaDTO(
      text:
          'Epic grants you a personal, non-exclusive, non-transferable, non-sublicensable limited right and license to use and install the Software on an Authorized Device (the <b>“License”</b>).  The rights that Epic grants you under the License are subject to the terms of this Agreement, and you may only make use of the License if you comply with all applicable terms.',
      type: _EulaDataType.paragraph,
    ),
    EulaDTO(
      text:
          'The License becomes effective on the date you accept this Agreement. The Software is licensed, not sold, to you under the License. The License does not grant you any title or ownership in the Software. ',
      type: _EulaDataType.paragraph,
    ),
    EulaDTO(
      text: '2. License Conditions',
      type: _EulaDataType.header,
    ),
    EulaDTO(
      text:
          'You may not do any of the following with respect to the Software or any of its parts:  (a) copy, reproduce, distribute, display, or use it in a way that is not expressly authorized in this Agreement; (b) sell, rent, lease, license, distribute, or otherwise transfer it; (c) reverse engineer, derive source code from, modify, adapt, translate, decompile, or disassemble it or make derivative works based on it; (d) remove, disable, circumvent, or modify any proprietary notice or label or security technology included in it; (e) create, develop, distribute, or use any unauthorized software programs; (f) use it to infringe or violate the rights of any third party, including but not limited to any intellectual property, publicity, or privacy rights; (g) use, export, or re-export it in violation of any applicable law or regulation; (h) use it on a device other than an Authorized Device; or (i) behave in a manner which is detrimental to the enjoyment of the Software or any Apps by other users as intended by Epic, in Epic’s sole judgment, including but not limited to the following – harassment, use of abusive or offensive language, game abandonment, game sabotage, spamming, social engineering, or scamming.',
      type: _EulaDataType.paragraph,
    ),
    EulaDTO(
      text: '3. Updates and Patches',
      type: _EulaDataType.header,
    ),
    EulaDTO(
      text:
          'Epic may provide patches, updates, or upgrades to the Software that must be installed in order for you to continue to use the Software or Services.  Epic may update the Software remotely without notifying you, and you hereby consent to Epic applying patches, updates, and upgrades.  Epic may modify, suspend, discontinue, substitute, replace, or limit your access to any aspect of the Software or Services at any time.  You acknowledge that your use of the Software or Services does not confer on you any interest, monetary or otherwise, in any aspect or feature of the Software or any Apps.  Epic does not have any maintenance or support obligations with respect to the Software or Services.',
      type: _EulaDataType.paragraph,
    ),
    EulaDTO(
      text: '4. Feedback',
      type: _EulaDataType.header,
    ),
    EulaDTO(
      text:
          'If you provide Epic with any Feedback, you hereby grant Epic a non-exclusive, fully-paid, royalty-free, irrevocable, perpetual, transferable, sublicensable license to reproduce, distribute, modify, prepare derivative works based on, publicly perform, publicly display, make, have made, use, sell, offer to sell, import, and otherwise exploit that Feedback for any purposes, for all current and future methods and forms of exploitation in any country.  If any such rights may not be licensed under applicable law (such as moral and other personal rights), you hereby waive and agree not to assert all such rights.  You understand and agree that Epic is not required to make any use of any Feedback that you provide.  You agree that if Epic makes use of your Feedback, Epic is not required to credit or compensate you for your contribution. You represent and warrant that you have sufficient rights in any Feedback that you provide to Epic to grant Epic and other affected parties the rights described above.  This includes but is not limited to intellectual property rights and other proprietary or personal rights.',
      type: _EulaDataType.paragraph,
    ),
    EulaDTO(
      text: '5. Ownership/Third Party Licenses',
      type: _EulaDataType.header,
    ),
    EulaDTO(
      text:
          'Epic and its licensors own all title, ownership rights, and intellectual property rights in the Software and Services.  Epic, Epic Games, Unreal, Unreal Engine, and their respective logos, are trademarks or registered trademarks of Epic and its affiliates in the United States of America and elsewhere.  All rights granted to you under this Agreement are granted by express license only and not by sale.  No license or other rights shall be created hereunder by implication, estoppel, or otherwise.',
      type: _EulaDataType.paragraph,
    ),
    EulaDTO(
      text:
          'The Software includes certain components provided by Epic’s licensors.  A list of credits and notices for third party components may be found in the Software interface.',
      type: _EulaDataType.paragraph,
    ),
    EulaDTO(
      text: '6. Disclaimers and Limitation of Liability',
      type: _EulaDataType.header,
    ),
    EulaDTO(
      text:
          '<b>The Software and Services is provided on an “as is” and “as available” basis, “with all faults” and without warranty of any kind.  Epic, its licensors, and its and their affiliates disclaim all warranties, conditions, common law duties, and representations (express, implied, oral, and written) with respect to the Software and Services, including without limitation all express, implied, and statutory warranties and conditions of any kind, such as title, non-interference with your enjoyment, authority, non-infringement, merchantability, fitness or suitability for any purpose (whether or not Epic knows or has reason to know of any such purpose), system integration, accuracy or completeness, results, reasonable care, workmanlike effort, lack of negligence, and lack of viruses, whether alleged to arise under law, by reason of custom or usage in the trade, or by course of dealing.  Without limiting the generality of the foregoing, Epic, its licensors, and its and their affiliates make no warranty that (1) the Software or Services will operate properly, (2) that the Software or Services will meet your requirements, (3) that the operation of the Software or Services will be uninterrupted, bug free, or error free in any or all circumstances, or (4) that any defects in the Software or Services can or will be corrected.  Any warranty against infringement that may be provided in Section 2-312 of the Uniform Commercial Code or in any other comparable statute is expressly disclaimed.  Epic, its licensors, and its and their affiliates do not guarantee continuous, error-free, virus-free, or secure operation of or access to the Software or Services.  This paragraph will apply to the maximum extent permitted by applicable law.<\/b>',
      type: _EulaDataType.paragraph,
    ),
    EulaDTO(
      text:
          '<b>To the maximum extent permitted by applicable law, neither Epic, nor its licensors, nor its or their affiliates, nor any of Epic’s service providers (collectively, the “Epic Parties”), shall be liable in any way for any loss of profits or any indirect, incidental, consequential, special, punitive, or exemplary damages, arising out of or in connection with this Agreement or the Software or Services, or the delay or inability to use or lack of functionality of the Software or Services, even in the event of an Epic Party’s fault, tort (including negligence), strict liability, indemnity, product liability, breach of contract, breach of warranty, or otherwise and even if an Epic Party has been advised of the possibility of such damages.  Further, to the maximum extent permitted by applicable law, the aggregate liability of the Epic Parties arising out of or in connection with this Agreement or the Software  or Services will not exceed the greater of (a) the total amounts you have paid (if any) to Epic for the Software or within the Software during the twelve (12) months immediately preceding the events giving rise to such liability and (b) U.S. \$5.00. These limitations and exclusions regarding damages apply even if any remedy fails to provide adequate compensation.<\/b>',
      type: _EulaDataType.paragraph,
    ),
    EulaDTO(
      text:
          '<b>Notwithstanding the foregoing, some countries, states, provinces or other jurisdictions do not allow the exclusion of certain warranties or the limitation of liability as stated above, so the above terms may not apply to you. Instead, in such jurisdictions, the foregoing exclusions and limitations shall apply only to the extent permitted by the laws of such jurisdictions. Also, you may have additional legal rights in your jurisdiction, and nothing in this Agreement will prejudice the statutory rights that you may have as a consumer of the Software or Services.<\/b>',
      type: _EulaDataType.paragraph,
    ),
    EulaDTO(
      text: '7. Indemnity',
      type: _EulaDataType.header,
    ),
    EulaDTO(
      text:
          'You agree to indemnify, pay the defense costs of, and hold Epic, its licensors, its and their affiliates, and its and their employees, officers, directors, agents, contractors, and other representatives harmless from all claims, demands, actions, losses, liabilities, and expenses (including attorneys’ fees, costs, and expert witnesses’ fees) that arise from or in connection with (a) any claim that, if true, would constitute a breach by you of this Agreement or negligence by you, or (b) any act or omission by you in using the Software or Services.  You agree to reimburse Epic on demand for any defense costs incurred by Epic and any payments made or loss suffered by Epic, whether in a court judgment or settlement, based on any matter covered by this Section 7.',
      type: _EulaDataType.paragraph,
    ),
    EulaDTO(
      text:
          'If you are prohibited by law from entering into the indemnification obligation above, then you assume, to the extent permitted by law, all liability for all claims, demands, actions, losses, liabilities, and expenses (including attorneys’ fees, costs and expert witnesses’ fees) that are the stated subject matter of the indemnification obligation above.',
      type: _EulaDataType.paragraph,
    ),
    EulaDTO(
      text: '8. Termination',
      type: _EulaDataType.header,
    ),
    EulaDTO(
      text:
          'Without limiting any other rights of Epic, this Agreement will terminate automatically without notice if you fail to comply with any of its terms and conditions.  You may also terminate this Agreement by deleting the Software.  Upon any termination, the License will automatically terminate, you may no longer exercise any of the rights granted to you by the License, and you must destroy all copies of the Software in your possession.',
      type: _EulaDataType.paragraph,
    ),
    EulaDTO(
      text:
          '<b>Except to the extent required by law, all payments and fees are non-refundable under all circumstances, regardless of whether or not this Agreement has been terminated.<\/b>',
      type: _EulaDataType.paragraph,
    ),
    EulaDTO(
      text: 'Sections 2, 6-11, and 13-15 will survive any termination of this Agreement.',
      type: _EulaDataType.paragraph,
    ),
    EulaDTO(
      text: '9. Governing Law and Jurisdiction ',
      type: _EulaDataType.header,
    ),
    EulaDTO(
      text:
          'You agree that this Agreement will be deemed to have been made and executed in the State of North Carolina, U.S.A., and any dispute will be resolved in accordance with the laws of North Carolina, excluding that body of law related to choice of laws, and of the United States of America.  Any action or proceeding brought to enforce the terms of this Agreement or to adjudicate any dispute must be brought in the Superior Court of Wake County, State of North Carolina or the United States District Court for the Eastern District of North Carolina.  You agree to the exclusive jurisdiction and venue of these courts.  You waive any claim of inconvenient forum and any right to a jury trial.  The Convention on Contracts for the International Sale of Goods will not apply.  Any law or regulation which provides that the language of a contract shall be construed against the drafter will not apply to this Agreement.',
      type: _EulaDataType.paragraph,
    ),
    EulaDTO(
      text: '10. Class Action Waiver',
      type: _EulaDataType.header,
    ),
    EulaDTO(
      text:
          '<b>You agree not to bring or participate in a class or representative action, private attorney general action, or collective arbitration related to the Software or Services or this Agreement.  You also agree not to seek to combine any action or arbitration related to the Software or Services or this Agreement with any other action or arbitration without the consent of all parties to this Agreement and all other actions or arbitrations.<\/b>',
      type: _EulaDataType.paragraph,
    ),
    EulaDTO(
      text: '11. U.S. Government Matters',
      type: _EulaDataType.header,
    ),
    EulaDTO(
      text:
          'The Software is a “Commercial Item” (as defined at 48 C.F.R. §2.101), consisting of “Commercial Computer Software” and “Commercial Computer Software Documentation” (as used in 48 C.F.R. §12.212 or 48 C.F.R. §227.7202, as applicable).  The Software is being licensed to U.S. Government end users only as Commercial Items and with only those rights as are granted to other licensees under this Agreement.',
      type: _EulaDataType.paragraph,
    ),
    EulaDTO(
      text:
          'You represent and warrant to Epic that you are not located in a country that is subject to a U.S. Government embargo or that has been designated by the U.S. Government as a “terrorist supporting” country, and that you are not listed on any U.S. Government list of prohibited or restricted parties.',
      type: _EulaDataType.paragraph,
    ),
    EulaDTO(
      text: '12. Amendments of this Agreement ',
      type: _EulaDataType.header,
    ),
    EulaDTO(
      text:
          '<b>Epic may issue an amended Agreement, Terms of Service, or Privacy Policy at any time in its discretion by posting the amended Agreement, Terms of Service, or Privacy Policy on its website or by providing you with digital access to amended versions of any of these documents when you next access the Software.  If any amendment to this Agreement, the Terms of Service, or the Privacy Policy is not acceptable to you, you may terminate this Agreement and must stop using the Software.  Your continued use of the Software will demonstrate your acceptance of the amended Agreement, Terms of Service, and Privacy Policy.<\/b>',
      type: _EulaDataType.paragraph,
    ),
    EulaDTO(
      text: '13. No Assignment',
      type: _EulaDataType.header,
    ),
    EulaDTO(
      text:
          'You may not, without the prior written consent of Epic, assign, transfer, charge, or sub-contract all or any of your rights or obligations under this Agreement, and any attempt without that consent will be null and void.  If restrictions on transfer of the Software in this Agreement are not enforceable under the law of your country, then this Agreement will be binding on any recipient of the Software.  Epic may at any time assign, transfer, charge, or sub-contract all or any of its rights or obligations under this Agreement.',
      type: _EulaDataType.paragraph,
    ),
    EulaDTO(
      text: '14. Definitions',
      type: _EulaDataType.header,
    ),
    EulaDTO(
      text: 'As used in this Agreement, the following capitalized words have the following meanings: ',
      type: _EulaDataType.paragraph,
    ),
    EulaDTO(
      text:
          '<b>“Authorized Device”<\/b> means a device on which the Software is designed and designated by Epic to be operated. An Authorized Device does not include a virtual machine or other emulation of such a mobile device.',
      type: _EulaDataType.paragraph,
    ),
    EulaDTO(
      text: '<b>“Epic”<\/b> means, depending on the location of your primary residence:',
      type: _EulaDataType.paragraph,
      subs: [
        EulaDTO(
            text:
                'Epic Games, Inc., a Maryland Corporation having its principal business offices at Box 254, 2474 Walnut Street, Cary, North Carolina, 27518, U.S.A.; or',
            type: _EulaDataType.sub,
            numbering: 'a.'),
        EulaDTO(
          text:
              'Epic Games Epic Games Commerce, GmbH, a limited liability company organized under the laws of Switzerland, having its principal business offices at Platz 10, 6039 Root D4, Switzerland.  ',
          type: _EulaDataType.sub,
          numbering: 'b.',
        ),
      ],
    ),
    EulaDTO(
      text:
          '<b>“Feedback”<\/b> means any feedback or suggestions that you provide to Epic regarding the Software, Services, or other Epic products and services.',
      type: _EulaDataType.paragraph,
    ),
    EulaDTO(
      text:
          '<b>“Services”<\/b> means any services made available to you through the Software, including services to manage your Epic account, connect to third party services.',
      type: _EulaDataType.paragraph,
    ),
    EulaDTO(
      text: '<b>“Software”<\/b> means Epic’s proprietary software known as Unreal Stage.',
      type: _EulaDataType.paragraph,
    ),
    EulaDTO(
      text: '15. Miscellaneous',
      type: _EulaDataType.header,
    ),
    EulaDTO(
      text:
          'This Agreement and any document or information referred to in this Agreement constitute the entire agreement between you and Epic relating to the subject matter covered by this Agreement.  All other communications, proposals, and representations with respect to the subject matter covered by this Agreement are excluded.',
      type: _EulaDataType.paragraph,
    ),
    EulaDTO(
      text:
          'The original of this Agreement is in English; any translations are provided for reference purposes only.  You waive any right you may have under the law of your country to have this Agreement written or construed in the language of any other country.',
      type: _EulaDataType.paragraph,
    ),
    EulaDTO(
      text:
          'This Agreement describes certain legal rights. You may have other rights under the laws of your jurisdiction. This Agreement does not change your rights under the laws of your jurisdiction if the laws of your jurisdiction do not permit it to do so.  Limitations and exclusions of warranties and remedies in this Agreement may not apply to you because your jurisdiction may not allow them in your particular circumstance.  In the event that certain provisions of this Agreement are held by a court or tribunal of competent jurisdiction to be unenforceable, those provisions shall be enforced only to the furthest extent possible under applicable law and the remaining terms of this Agreement will remain in full force and effect.',
      type: _EulaDataType.paragraph,
    ),
    EulaDTO(
      text:
          'Any act by Epic to exercise, or failure or delay in exercise of, any of its rights under this Agreement, at law or in equity will not be deemed a waiver of those or any other rights or remedies available in contract, at law or in equity.',
      type: _EulaDataType.paragraph,
    ),
    EulaDTO(
      text:
          'You agree that this Agreement does not confer any rights or remedies on any person other than the parties to this Agreement, except as expressly stated.',
      type: _EulaDataType.paragraph,
    ),
    EulaDTO(
      text:
          'Epic’s obligations are subject to existing laws and legal process, and Epic may comply with law enforcement or regulatory requests or requirements despite any contrary term in this Agreement.',
      type: _EulaDataType.paragraph,
    ),
  ];
}
