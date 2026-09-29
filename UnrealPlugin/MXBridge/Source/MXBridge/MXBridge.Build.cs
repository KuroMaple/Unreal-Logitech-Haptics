using UnrealBuildTool;

public class MXBridge : ModuleRules
{
	public MXBridge(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"UnrealEd",     // GEditor, FScopedTransaction, editor settings
			"LevelEditor",  // active level viewport
			"Slate",
			"SlateCore",
			"HTTPServer",   // FHttpServerModule
			"Json",
			"LiveCoding",   // ILiveCodingModule (Win64 only, matching the .uplugin allow list)
		});
	}
}
