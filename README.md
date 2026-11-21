# Épingle

Garde n'importe quelle fenêtre au premier plan avec un raccourci clavier.

![Aperçu de la bordure animée](docs/apercu_bordure.png)

- **Alt + ²** (la touche sous Échap) épingle la fenêtre active. Refaites-le pour la libérer.
- Une fenêtre épinglée est entourée d'une lueur animée bleu/vert. Au moment de l'épinglage, la bordure fait un flash clair, puis le dégradé se met à tourner.
- Plusieurs fenêtres peuvent être épinglées en même temps (jusqu'à 16).

## Utilisation

1. Lancez `Epingle.exe`. Aucune installation n'est nécessaire. Une icône apparaît près de l'horloge.
2. Cliquez sur une fenêtre puis appuyez sur **Alt + ²**.
3. Cliquez sur l'icône (clic gauche ou droit) pour accéder au menu :
   - **Modifier le raccourci…** : appuyez sur la nouvelle combinaison. *Échap* annule et *Suppr* rétablit Alt + ².
   - **Libérer toutes les fenêtres**
   - **Lancer au démarrage de Windows** : désactivé par défaut.
   - **Quitter** : les fenêtres épinglées sont alors libérées.

Windows ne permet pas d'épingler une fenêtre d'un programme lancé *en administrateur* depuis un programme qui ne l'est pas. Épingle vous le signale par une notification.

## Confidentialité et permissions

- **Aucun accès réseau.** Le programme n'importe aucune bibliothèque réseau. Pour le vérifier, la liste complète des fonctions Windows utilisées se trouve dans `src/win.h`. Vous pouvez aussi lancer `dumpbin /imports Epingle.exe` : vous n'y trouverez que kernel32, user32, gdi32, shell32 et advapi32. `dwmapi` et `shcore` sont chargées à la demande, uniquement depuis System32, ce qui protège contre une DLL piégée placée à côté du .exe.
- **Droits utilisateur standard** (`asInvoker`) : l'application ne demande jamais l'administrateur.
- **Aucune injection** dans les autres programmes. La position des fenêtres est suivie avec un hook « hors contexte », actif seulement tant qu'une fenêtre est épinglée.
- **Réglages** : `HKEY_CURRENT_USER\Software\Epingle` (le raccourci). L'option de démarrage écrit dans `HKCU\...\CurrentVersion\Run`. Rien d'autre n'est écrit sur le disque.
- **Légèreté** : un exécutable de 60 Ko, sans runtime C ni dépendance. Sans fenêtre épinglée, l'app ne consomme aucun CPU. Dès qu'une fenêtre est épinglée, l'animation tourne à environ 30 images/s.

## Désinstaller

Quittez l'app, décochez « Lancer au démarrage » si besoin, puis supprimez l'exe. Optionnel : supprimez la clé `HKCU\Software\Epingle`.

## Compiler

Seul LLVM est nécessaire (clang, lld-link, llvm-rc, llvm-dlltool). Ni Visual Studio ni le SDK Windows ne sont requis :

```sh
./build.sh          # produit build/Epingle.exe
```

Sous Windows, installez LLVM (`winget install LLVM.LLVM`) puis lancez le script depuis Git Bash.
L'icône se régénère avec `python tools/make_icon.py src/app.ico` (nécessite Pillow).

Windows 10 ou 11, 64 bits.
